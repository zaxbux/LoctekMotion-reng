#define PACKET_MAGIC_VALUE 0x9B
#define PACKET_SENTINEL_VALUE 0x9D

enum MessageType
{
	/** Button state */
	HSx02 = 0x02,
	/** Unknown*/
	CBx11 = 0x11,
	/** Display*/
	CBx12 = 0x12,
	/** Buzzer */
	CBx14 = 0x14,
	/** Normal */
	CBx15 = 0x15,
};