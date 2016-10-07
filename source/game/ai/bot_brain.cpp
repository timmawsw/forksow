#include "bot.h"
#include "ai_ground_trace_cache.h"
#include "ai_squad_based_team_brain.h"
#include "bot_brain.h"
#include "tactical_spots_registry.h"
#include <algorithm>
#include <limits>
#include <stdarg.h>

void BotBrain::EnemyPool::OnEnemyRemoved(const Enemy *enemy)
{
    bot->ai->botRef->OnEnemyRemoved(enemy);
}

void BotBrain::EnemyPool::OnNewThreat(const edict_t *newThreat)
{
    bot->ai->botRef->botBrain.OnNewThreat(newThreat, this);
}

void BotBrain::OnAttachedToSquad(AiSquad *squad_)
{
    this->squad = squad_;
    activeEnemyPool = squad_->EnemyPool();
    ClearPlan();
}

void BotBrain::OnDetachedFromSquad(AiSquad *squad_)
{
    if (this->squad != squad_)
    {
        FailWith("was not attached to squad %s", squad_ ? squad_->Tag() : "???");
    }
    this->squad = nullptr;
    activeEnemyPool = &botEnemyPool;
    ClearPlan();
}

void BotBrain::OnAttitudeChanged(const edict_t *ent, int oldAttitude_, int newAttitude_)
{
    if (oldAttitude_ < 0 && newAttitude_ >= 0)
    {
        botEnemyPool.Forget(ent);
        if (squad)
            squad->EnemyPool()->Forget(ent);
    }
}

void BotBrain::OnNewThreat(const edict_t *newThreat, const AiFrameAwareUpdatable *threatDetector)
{
    // Reject threats detected by bot brain if there is active squad.
    // Otherwise there may be two calls for a single or different threats
    // detected by squad and the bot brain enemy pool itself.
    if (squad && threatDetector == &this->botEnemyPool)
        return;

    vec3_t botLookDir;
    AngleVectors(self->s.angles, botLookDir, nullptr, nullptr);
    Vec3 toEnemyDir = Vec3(newThreat->s.origin) - self->s.origin;
    float squareDistance = toEnemyDir.SquaredLength();
    if (squareDistance > 1)
    {
        float distance = 1.0f / Q_RSqrt(squareDistance);
        toEnemyDir *= distance;
        if (toEnemyDir.Dot(botLookDir) < 0)
        {
            // Try to guess enemy origin
            toEnemyDir.X() += -0.25f + 0.50f * random();
            toEnemyDir.Y() += -0.10f + 0.20f * random();
            toEnemyDir.NormalizeFast();
            VectorCopy(self->s.origin, threatPossibleOrigin.Data());
            threatPossibleOrigin += distance * toEnemyDir;
            threatDetectedAt = level.time;
        }
    }
}

void BotBrain::OnEnemyRemoved(const Enemy *enemy)
{
    if (selectedEnemies.AreValid())
    {
        if (selectedEnemies.Contain(enemy))
        {
            selectedEnemies.Invalidate();
            ClearPlan();
        }
    }
}

BotBrain::BotBrain(Bot *bot, float skillLevel_)
    : AiBaseBrain(bot->self),
      bot(bot->self),
      baseOffensiveness(0.5f),
      skillLevel(skillLevel_),
      reactionTime(320 - From0UpToMax(300, BotSkill())),
      nextTargetChoiceAt(level.time),
      targetChoicePeriod(800 - From0UpToMax(300, BotSkill())),
      itemsSelector(bot->self),
      selectedNavEntity(nullptr, std::numeric_limits<float>::max(), 0),
      prevSelectedNavEntity(nullptr),
      threatPossibleOrigin(NAN, NAN, NAN),
      threatDetectedAt(0),
      botEnemyPool(bot->self, this, skillLevel_),
      selectedEnemies(bot->selectedEnemies),
      selectedWeapons(bot->selectedWeapons)
{
    squad = nullptr;
    activeEnemyPool = &botEnemyPool;
    SetTag(bot->self->r.client->netname);
}

void BotBrain::Frame()
{
    // Call superclass method first
    AiBaseBrain::Frame();

    // Reset offensiveness to a default value
    if (G_ISGHOSTING(self))
        baseOffensiveness = 0.5f;

    botEnemyPool.Update();
}

void BotBrain::Think()
{
    if (selectedEnemies.AreValid())
    {
        if (level.time - selectedEnemies.LastSeenAt() >= reactionTime)
        {
            selectedEnemies.Invalidate();
            UpdateSelectedEnemies();
            UpdateBlockedAreasStatus();
        }
    }
    else
    {
        UpdateSelectedEnemies();
        UpdateBlockedAreasStatus();
    }

    // This call will try to find a plan if it is not present
    AiBaseBrain::Think();
}

void BotBrain::UpdateSelectedEnemies()
{
    selectedEnemies.primaryEnemy = nullptr;
    selectedEnemies.activeEnemies.clear();
    selectedEnemies.timeoutAt = level.time;
    if (const Enemy *primaryEnemy = activeEnemyPool->ChooseAimEnemy(self))
    {
        selectedEnemies.primaryEnemy = primaryEnemy;
        for (const Enemy *activeEnemy: activeEnemyPool->ActiveEnemies())
            selectedEnemies.activeEnemies.push_back(activeEnemy);
        selectedEnemies.timeoutAt += targetChoicePeriod;
    }
}

void BotBrain::OnEnemyViewed(const edict_t *enemy)
{
    botEnemyPool.OnEnemyViewed(enemy);
    if (squad)
        squad->OnBotViewedEnemy(self, enemy);
}

void BotBrain::OnPain(const edict_t *enemy, float kick, int damage)
{
    botEnemyPool.OnPain(self, enemy, kick, damage);
    if (squad)
        squad->OnBotPain(self, enemy, kick, damage);
}

void BotBrain::OnEnemyDamaged(const edict_t *target, int damage)
{
    botEnemyPool.OnEnemyDamaged(self, target, damage);
    if (squad)
        squad->OnBotDamagedEnemy(self, target, damage);
}

void BotBrain::UpdateBlockedAreasStatus()
{
    if (activeEnemyPool->ActiveEnemies().empty() || ShouldRushHeadless())
    {
        // Reset all possibly blocked areas
        RouteCache()->SetDisabledRegions(nullptr, nullptr, 0, DroppedToFloorAasAreaNum());
        return;
    }

    // Wait for enemies selection, do not perform cache flushing
    // which is extremely expensive and are likely to be overridden next frame
    if (!selectedEnemies.AreValid())
        return;

    StaticVector<Vec3, EnemyPool::MAX_TRACKED_ENEMIES> mins;
    StaticVector<Vec3, EnemyPool::MAX_TRACKED_ENEMIES> maxs;

    AiGroundTraceCache *groundTraceCache = AiGroundTraceCache::Instance();
    for (const Enemy *enemy: selectedEnemies)
    {
        if (selectedEnemies.IsPrimaryEnemy(enemy) && WillAttackMelee())
            continue;

        // TODO: This may act as cheating since actual enemy origin is used.
        // This is kept for conformance to following ground trace check.
        float squareDistance = DistanceSquared(self->s.origin, enemy->ent->s.origin);
        // (Otherwise all nearby paths are treated as blocked by enemy)
        if (squareDistance < 72.0f)
            continue;
        float distance = 1.0f / Q_RSqrt(squareDistance);
        float side = 24.0f + 96.0f * BoundedFraction(distance - 72.0f, 4 * 384.0f);

        // Try to drop an enemy origin to floor
        // TODO: AiGroundTraceCache interface forces using an actual enemy origin
        // and not last seen one, so this may act as cheating.
        vec3_t origin;
        // If an enemy is close to ground (an origin may and has been dropped to floor)
        if (groundTraceCache->TryDropToFloor(enemy->ent, 128.0f, origin, level.time - enemy->LastSeenAt()))
        {
            // Do not use bounds lower than origin[2] (except some delta)
            mins.push_back(Vec3(-side, -side, -8.0f) + origin);
            maxs.push_back(Vec3(+side, +side, 128.0f) + origin);
        }
        else
        {
            // Use a bit lower bounds (an enemy is likely to fall down)
            mins.push_back(Vec3(-side, -side, -192.0f) + origin);
            maxs.push_back(Vec3(+side, +side, +108.0f) + origin);
        }
    }

    // getting mins[0] address for an empty vector in debug will trigger an assertion
    if (!mins.empty())
        RouteCache()->SetDisabledRegions(&mins[0], &maxs[0], mins.size(), DroppedToFloorAasAreaNum());
    else
        RouteCache()->SetDisabledRegions(nullptr, nullptr, 0, DroppedToFloorAasAreaNum());
}

void BotBrain::PrepareCurrWorldState(WorldState *worldState)
{
    worldState->SetIgnoreAll(false);

    if (selectedEnemies.AreValid())
    {
        worldState->HasEnemy().SetValue(true);
        worldState->HasThreateningEnemy().SetValue(selectedEnemies.AreThreatening());
        worldState->RawDamageToKill().SetValue((short)selectedEnemies.DamageToKill());
        worldState->EnemyHasQuad().SetValue(selectedEnemies.HaveQuad());
        float distanceToEnemy = DistanceFast(self->s.origin, selectedEnemies.LastSeenOrigin().Data());
        worldState->DistanceToEnemy().SetValue(distanceToEnemy);
    }
    else
    {
        worldState->HasEnemy().SetValue(false);
        worldState->HasThreateningEnemy().SetValue(false);
        worldState->RawDamageToKill().SetIgnore(true);
        worldState->EnemyHasQuad().SetIgnore(true);
        worldState->DistanceToEnemy().SetIgnore(true);
    }

    worldState->Health().SetValue((short)HEALTH_TO_INT(self->health));
    worldState->Armor().SetValue(self->r.client->ps.stats[STAT_ARMOR]);

    worldState->HasQuad().SetValue(::HasQuad(self));
    worldState->HasShell().SetValue(::HasShell(self));

    const SelectedNavEntity &currSelectedNavEntity = GetOrUpdateSelectedNavEntity();
    if (currSelectedNavEntity.IsEmpty())
    {
        worldState->HasGoalItemNavTarget().SetIgnore(true);
        worldState->DistanceToGoalItemNavTarget().SetIgnore(true);
        worldState->GoalItemWaitTime().SetIgnore(true);
    }
    else
    {
        worldState->HasGoalItemNavTarget().SetValue(true);
        const NavEntity *navEntity = currSelectedNavEntity.GetNavEntity();
        float distanceToNavTarget = DistanceFast(self->s.origin, navEntity->Origin().Data());
        worldState->DistanceToGoalItemNavTarget().SetValue(distanceToNavTarget);
        // Find a travel time to the goal itme nav entity in milliseconds
        // We hope this router call gets cached by AAS subsystem
        unsigned travelTime = 10U * FindTravelTimeToGoalArea(navEntity->AasAreaNum());
        // AAS returns 1 seconds^-2 as a lowest feasible value
        if (travelTime <= 10)
            travelTime = 0;
        unsigned spawnTime = navEntity->SpawnTime();
        // If the goal item spawns before the moment when it gets reached
        if (level.time + travelTime >= spawnTime)
            worldState->GoalItemWaitTime().SetValue(0);
        else
            worldState->GoalItemWaitTime().SetValue(spawnTime - level.time - travelTime);
    }

    worldState->HasJustPickedGoalItem().SetValue(HasJustPickedGoalItem());

    recentWorldState = *worldState;
}

float BotBrain::GetEffectiveOffensiveness() const
{
    if (!squad)
    {
        if (selectedEnemies.AreValid() && selectedEnemies.HaveCarrier())
            return 0.65f + 0.35f * decisionRandom;
        return baseOffensiveness;
    }
    return squad->IsSupporter(self) ? 1.0f : 0.0f;
}


