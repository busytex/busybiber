#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef LC_ALL

int
main (const int argc, const char ** argv)
{
}

#else
#  define C_ARRAY_LENGTH(a)  (sizeof(a)/sizeof((a)[0]))
#  define strEQ(s1,s2) (strcmp(s1,s2) == 0)
#  define strNE(s1,s2) (strcmp(s1,s2) != 0)
#  define Copy(s,d,n,t) (void)memcpy((char*)(d),(const char*)(s), (n) * sizeof(t))
#  define memEQ(s1,s2,l) (memcmp(((const void *) (s1)), ((const void *) (s2)), l) == 0)
#  define memNE(s1,s2,l) (! memEQ(s1,s2,l))

int
main (const int argc, const char ** argv)
{

    int debug = 0;

    /* All categories Perl knows about on any system.  If any are missing, this
     * won't work for that system, and they must be added here and in perl.h,
     * locale.c */
    const int categories[] = {

#  ifdef LC_CTYPE
        LC_CTYPE,
#  endif
#  ifdef LC_NUMERIC
        LC_NUMERIC,
#  endif
#  ifdef LC_COLLATE
        LC_COLLATE,
#  endif
#  ifdef LC_TIME
        LC_TIME,
#  endif
#  ifdef LC_MESSAGES
        LC_MESSAGES,
#  endif
#  ifdef LC_MONETARY
        LC_MONETARY,
#  endif
#  ifdef LC_ADDRESS
        LC_ADDRESS,
#  endif
#  ifdef LC_IDENTIFICATION
        LC_IDENTIFICATION,
#  endif
#  ifdef LC_MEASUREMENT
        LC_MEASUREMENT,
#  endif
#  ifdef LC_PAPER
        LC_PAPER,
#  endif
#  ifdef LC_TELEPHONE
        LC_TELEPHONE,
#  endif
#  ifdef LC_NAME
        LC_NAME,
#  endif
#  ifdef LC_SYNTAX
        LC_SYNTAX,
#  endif
#  ifdef LC_TOD
        LC_TOD
#  endif

    };

    const char * category_names[] = {

#  ifdef LC_CTYPE
        "LC_CTYPE",
#  endif
#  ifdef LC_NUMERIC
        "LC_NUMERIC",
#  endif
#  ifdef LC_COLLATE
        "LC_COLLATE",
#  endif
#  ifdef LC_TIME
        "LC_TIME",
#  endif
#  ifdef LC_MESSAGES
        "LC_MESSAGES",
#  endif
#  ifdef LC_MONETARY
        "LC_MONETARY",
#  endif
#  ifdef LC_ADDRESS
        "LC_ADDRESS",
#  endif
#  ifdef LC_IDENTIFICATION
        "LC_IDENTIFICATION",
#  endif
#  ifdef LC_MEASUREMENT
        "LC_MEASUREMENT",
#  endif
#  ifdef LC_PAPER
        "LC_PAPER",
#  endif
#  ifdef LC_TELEPHONE
        "LC_TELEPHONE",
#  endif
#  ifdef LC_NAME
        "LC_NAME",
#  endif
#  ifdef LC_SYNTAX
        "LC_SYNTAX",
#  endif
#  ifdef LC_TOD
        "LC_TOD"
#  endif

    };

    char alternate[1024] = { '\0' } ;

    /* This is a list of locales that are likely to be found on any machine
     * (Windows and non-Windows) */
    const char * candidates[] = {
                                  "POSIX",
                                  "C.UTF-8",
                                  "en_US",
                                  "en_US.UTF-8",
                                  "American",
                                  "English"
                                };
    char separator[1024];
    size_t separator_len = 0;
    unsigned int distincts_count = 0;

    /* We look through the candidates for one which returns the same non-C
     * locale for every category */
    for (unsigned int i = 0; i < C_ARRAY_LENGTH(candidates); i++) {
        const char * candidate = candidates[i];
        distincts_count = 0;
        alternate[0] = '\0';

        for (unsigned int j = 0; j < C_ARRAY_LENGTH(categories); j++) {
            const int category = categories[j];
            const char * locale_name = setlocale(category, candidate);

            if (locale_name == NULL) {  /* Not on this system */
                break;
            }

            if (debug) fprintf(stderr,
                               "i=%d,j=%d;"
                               " Return of setlocale(%d=%s, '%s') is '%s'\n",
                               i, j,
                               category, category_names[j],
                               candidate, locale_name);

            /* If the candidate is indistinguishable from C, break to try the
             * next candidate */
            if (strEQ(locale_name, "C")) {
                break;
            }

            /* Save the name the first time through, and on subsequent ones */
            /* make sure the name is the same as before, so the code below can
             * be assured of finding it when searching */
            if (j == 0) {
                strncpy(alternate, locale_name, sizeof(alternate));
            }
            else if (strNE(alternate, locale_name)) {
                break;
            }

            distincts_count++;
        }

        /* Done with this candidate.  If every category returned the same non-C
         * name, this candidate works.  It not, loop to try the next candidate
         * */
        if (distincts_count == C_ARRAY_LENGTH(categories)) {
            break;
        }
    }

    /* Here, either found a suitable candidate, or exhausted the possibilities.
     * In the latter case, give up */
    if (distincts_count < C_ARRAY_LENGTH(categories)) {
        fprintf(stderr, "Couldn't find a locale distinguishable from C\n");
        return 1;
    }

    /* An example syntax, from cygwin, is:
     *     LC_COLLATE/LC_CTYPE/LC_MONETARY/LC_NUMERIC/LC_TIME/LC_MESSAGES
     * The locales for a given category are always in the same position,
     * indicated above, with a slash separating them */

    int map_LC_ALL_position_to_category[C_ARRAY_LENGTH(categories)];

    /* Initialize */
    for (unsigned int i = 0; i < C_ARRAY_LENGTH(categories); i++) {
        map_LC_ALL_position_to_category[i] = INT_MAX;
    }

    const char * lc_all = NULL;

    /* We need to find the category that goes in each position */
    for (unsigned int i = 0; i < C_ARRAY_LENGTH(categories); i++) {

        /* First set everything to 'C' */
        if (! setlocale(LC_ALL, "C")) {
            fprintf(stderr, "Failed to set LC_ALL to C\n");
            return 1;
        }

        /* Then set this category to the alternate */
        if (! setlocale(categories[i], alternate)) {
            fprintf(stderr, "Failed to set %d to to '%s'\n",
                            categories[i], alternate);
            return 1;
        }

        /* Then find what the system says LC_ALL looks like with just this one
         * category not set to 'C' */
        lc_all = setlocale(LC_ALL, NULL);
        if (! lc_all) {
            fprintf(stderr, "Failed to retrieve LC_ALL\n");
            return 1;
        }

        if (debug) fprintf(stderr, "LC_ALL is '%s'\n", lc_all);

        /* Assume is name=value pairs if the result contains both an equals and
         * a semi-colon. */
        if (strchr(lc_all, '=') && strchr(lc_all, ';')) {
            fprintf(stdout, "\"=;\"\n\n");
            return 0;
        }

        /* Here isn't name=value pairs.  Find the position of the alternate */
        const char * alt_pos = strstr(lc_all, alternate);
        if (! alt_pos) {
            fprintf(stderr, "Couldn't find '%s' in '%'s\n", alternate, lc_all);
            return 1;
        }

        /* Parse the LC_ALL string from the beginning up to where the alternate
         * locale is */
        const char * s = lc_all;
        int count = 0;
        while (s < alt_pos) {

            /* Count the 'C' locales before the non-C one.  (Note the letter
             * 'C' can only occur as the entire 'C' locale, since all of them
             * are that locale before 'alt_pos') */
            const char * next_C = (const char *) memchr(s, 'C', alt_pos - s);
            if (next_C) {
                count++;
                s = next_C + 1;
                continue;
            }

            /* Here, there is no 'C' between 's' and the alternate locale, so
             * 'count' gives the total number of occurrences of 'C' in that
             * span.  If count is 0, this is the first category in an LC_ALL
             * string, and we know its position, but not the separator. */
            if (count == 0) {
                break;
            }

            /* When 'count' isn't 0,  's' points to one past the previous 'C'.
             * The separator starts here, ending just before the non-C locale.
             */

            const char * new_sep = s;
            unsigned int new_sep_len = alt_pos - s;

            /* If we don't already have a separator saved, save this as it */
            if (separator_len == 0) {
                separator_len = new_sep_len;
                Copy(s, separator, separator_len, char);
                separator[separator_len] = '\0';
            }
            else {  /* Otherwise make sure it's the same string as previously
                     * calculated */
                if (   new_sep_len != separator_len
                    || memNE(separator, new_sep, separator_len))
                {
                    fprintf(stderr, "Unexpectedly got distinct separators"
                                    " '%s' vs '%s\n", separator, new_sep);
                    return 1;
                }
            }

            /* Here, we have found the position of category[i] in LC_ALL. */
            break;

        } /* End of loop parsing the LC_ALL string */

        if (map_LC_ALL_position_to_category[count] != INT_MAX) {
            fprintf(stderr, "Categories %d and %d both appear to occupy"
                            " position %d in LC_ALL; there is something"
                            " wrong with the calculation\n",
                            categories[count], categories[i],
                            count);
            return 1;
        }

        /* Save the position of this category */
        map_LC_ALL_position_to_category[count] = categories[i];

    } /* End of loop through all the categories */

    fprintf(stdout, "\"%s\"\n{", separator);
    for (unsigned int i = 0; i < C_ARRAY_LENGTH(categories) - 1; i++) {
        fprintf(stdout, " %d,", map_LC_ALL_position_to_category[i]);
    }
    fprintf(stdout, " %d }\n", map_LC_ALL_position_to_category[
                                                C_ARRAY_LENGTH(categories) - 1]);
    return 0;

}

#endif
