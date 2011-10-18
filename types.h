/* 
 * types.h
 * author: B. Jordan
 *
 * This file contains the type definitions for the myftp protocol.
 */

#define PUT_1 'P'
#define PUT_2 'Q'
#define PUT_STATUS_READY '0'
#define PUT_STATUS_WRITE_ERROR '1'
#define PUT_STATUS_PERM_ERROR '2'
#define PUT_STATUS_OTHER_ERROR '3'

#define GET_1 'G'
#define GET_2 'H'
#define GET_STATUS_READY '0'
#define GET_STATUS_READ_ERROR '1'
#define GET_STATUS_PERM_ERROR '2'
#define GET_STATUS_OTHER_ERROR '3'

#define PWD_1 'W'
#define PWD_STATUS_OKAY '0'
#define PWD_STATUS_ERROR '1'

#define DIR_1 'D'
#define DIR_STATUS_OKAY '0'
#define DIR_STATUS_ERROR '1'

#define CD_1 'C'
#define CD_STATUS_OKAY '0'
#define CD_STATUS_ERROR '1'

