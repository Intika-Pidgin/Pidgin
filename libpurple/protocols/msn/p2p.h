#ifndef MSN_P2P_H
#define MSN_P2P_H


#pragma pack(push,1)
typedef struct {
	guint32 session_id;
	guint32 id;
	guint64 offset;
	guint64 total_size;
	guint32 length;
	guint32 flags;
	guint32 ack_id;
	guint32 ack_sub_id;
	guint64 ack_size;
/*	guint8  body[1]; */
} MsnP2PHeader;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct {
	guint8  header_len;
	guint8  opcode;
	guint32 message_len;
	guint64 base_id;
} MsnP2Pv2Header;
#pragma pack(pop)

typedef struct
{
	guint32 value;
} MsnP2PFooter;

typedef enum
{
	P2P_NO_FLAG         = 0x0,        /**< No flags specified */
	P2P_OUT_OF_ORDER    = 0x1,        /**< Chunk out-of-order */
	P2P_ACK             = 0x2,        /**< Acknowledgement */
	P2P_PENDING_INVITE  = 0x4,        /**< There is a pending invite */
	P2P_BINARY_ERROR    = 0x8,        /**< Error on the binary level */
	P2P_MSN_OBJ_DATA    = 0x20,       /**< MsnObject data */
	P2P_WML2009_COMP    = 0x1000000,  /**< Compatibility with WLM 2009 */
	P2P_FILE_DATA       = 0x1000030   /**< File transfer data */
} MsnP2PHeaderFlag;
/* Info From:
 * http://msnpiki.msnfanatic.com/index.php/MSNC:P2Pv1_Headers#Flags
 * http://trac.kmess.org/changeset/ba04d0c825769d23370511031c47f6be75fe9b86
 * #7180
 */

#define P2P_PACKET_HEADER_SIZE sizeof(MsnP2PHeader)
#define P2P_PACKET_FOOTER_SIZE sizeof(MsnP2PFooter)

MsnP2PHeader *
msn_p2p_header_from_wire(MsnP2PHeader *wire);

MsnP2PHeader *
msn_p2p_header_to_wire(MsnP2PHeader *header);

MsnP2PFooter *
msn_p2p_footer_from_wire(MsnP2PFooter *wire);

MsnP2PFooter *
msn_p2p_footer_to_wire(MsnP2PFooter *footer);

#endif /* MSN_P2P_H */
