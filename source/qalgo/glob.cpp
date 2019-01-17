/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <stdio.h>
#include <ctype.h>
#include "glob.h"

#ifdef _WIN32
#pragma warning( disable : 4127 )       // conditional expression is constant
#endif

/* Like glob_match, but match PATTERN against any final segment of TEXT.  */
static int glob_match_after_star( const char *pattern, const char *text, const int casecmp ) {
	register const char *p = pattern, *t = text;
	register char c, c1;

	while( ( c = *p++ ) == '?' || c == '*' )
		if( c == '?' && *t++ == '\0' ) {
			return 0;
		}

	if( c == '\0' ) {
		return 1;
	}

	if( c == '\\' ) {
		c1 = *p;
	} else {
		c1 = c;
	}

	while( 1 ) {
		if( ( c == '[' || ( casecmp ? ( *t == c1 ) : ( tolower( *t ) == tolower( c1 ) ) ) ) && glob_match( p - 1, t, casecmp ) ) {
			return 1;
		}
		if( *t++ == '\0' ) {
			return 0;
		}
	}
}

/* Match the pattern PATTERN against the string TEXT;
return 1 if it matches, 0 otherwise.

A match means the entire string TEXT is used up in matching.

In the pattern string, `*' matches any sequence of characters,
`?' matches any character, [SET] matches any character in the specified set,
[!SET] matches any character not in the specified set.

A set is composed of characters or ranges; a range looks like
character hyphen character (as in 0-9 or A-Z).
[0-9a-zA-Z_] is the set of characters allowed in C identifiers.
Any other character in the pattern must be matched exactly.

To suppress the special syntactic significance of any of `[]*?!-\',
and match the character exactly, precede it with a `\'.
*/

int glob_match( const char *pattern, const char *text, const int casecmp ) {
	register const char *p = pattern, *t = text;
	register char c;

	while( ( c = *p++ ) != '\0' )
		switch( c ) {
			case '?':
				if( *t == '\0' ) {
					return 0;
				} else {
					++t;
				}
				break;

			case '\\':
				if( *p++ != *t++ ) {
					return 0;
				}
				break;

			case '*':
				return glob_match_after_star( p, t, casecmp );

			case '[':
			{
				register char c1 = *t++;
				int invert;

				if( !c1 ) {
					return ( 0 );
				}

				invert = ( ( *p == '!' ) || ( *p == '^' ) );
				if( invert ) {
					p++;
				}

				c = *p++;
				while( 1 ) {
					register char cstart = c, cend = c;

					if( c == '\\' ) {
						cstart = *p++;
						cend = cstart;
					}
					if( c == '\0' ) {
						return 0;
					}

					c = *p++;
					if( c == '-' && *p != ']' ) {
						cend = *p++;
						if( cend == '\\' ) {
							cend = *p++;
						}
						if( cend == '\0' ) {
							return 0;
						}
						c = *p++;
					}
					if( c1 >= cstart && c1 <= cend ) {
						goto match;
					}
					if( c == ']' ) {
						break;
					}
				}
				if( !invert ) {
					return 0;
				}
				break;

match:
				/* Skip the rest of the [...] construct that already matched.  */
				while( c != ']' ) {
					if( c == '\0' ) {
						return 0;
					}
					c = *p++;
					if( c == '\0' ) {
						return 0;
					} else if( c == '\\' ) {
						++p;
					}
				}
				if( invert ) {
					return 0;
				}
				break;
			}

			default:
				if( casecmp ) {
					if( c != *t++ ) {
						return 0;
					}
				} else {
					if( tolower( c ) != tolower( *t ) ) {
						return 0;
					}
					t++;
				}
		}

	return *t == '\0';
}
