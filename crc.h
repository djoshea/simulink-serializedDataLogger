#include<sys/types.h>
#include<inttypes.h>

typedef uint32_t uint32; 
typedef uint8_t uint8; 

extern uint32 calculate_crc(uint8 *data_blk_ptr, uint32 data_blk_size);
