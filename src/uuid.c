#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#if defined(__FreeBSD__)
#include <uuid.h>
#else
#include <uuid/uuid.h>
#endif

void
get_uuid(char *temp)
{
    char     *s;
    uuid_t    uu;

#if defined(__FreeBSD__)
    uuid_create(&uu, NULL);
    uuid_to_string(&uu, &s, 0);
#else
    s = (char *) malloc(37);	/* UUID is 36 chars + \0 */
    uuid_generate(uu);
    uuid_unparse(uu, s);
#endif
    memcpy(temp, s, 37);
}
