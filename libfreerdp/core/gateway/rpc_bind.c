/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RPC Secure Context Binding
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include "rpc_bind.h"

/**
 * Connection-Oriented RPC Protocol Client Details:
 * http://msdn.microsoft.com/en-us/library/cc243724/
 */

/* Syntax UUIDs */

const p_uuid_t TSGU_UUID =
{
	0x44E265DD, /* time_low */
	0x7DAF, /* time_mid */
	0x42CD, /* time_hi_and_version */
	0x85, /* clock_seq_hi_and_reserved */
	0x60, /* clock_seq_low */
	{ 0x3C, 0xDB, 0x6E, 0x7A, 0x27, 0x29 } /* node[6] */
};

#define TSGU_SYNTAX_IF_VERSION	0x00030001

const p_uuid_t NDR_UUID =
{
	0x8A885D04, /* time_low */
	0x1CEB, /* time_mid */
	0x11C9, /* time_hi_and_version */
	0x9F, /* clock_seq_hi_and_reserved */
	0xE8, /* clock_seq_low */
	{ 0x08, 0x00, 0x2B, 0x10, 0x48, 0x60 } /* node[6] */
};

#define NDR_SYNTAX_IF_VERSION	0x00000002

const p_uuid_t BTFN_UUID =
{
	0x6CB71C2C, /* time_low */
	0x9812, /* time_mid */
	0x4540, /* time_hi_and_version */
	0x03, /* clock_seq_hi_and_reserved */
	0x00, /* clock_seq_low */
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } /* node[6] */
};

#define BTFN_SYNTAX_IF_VERSION	0x00000001

/**
 * SECURE_BIND: RPC bind PDU with sec_trailer and auth_token. Auth_token is generated by calling
 * the implementation equivalent of the abstract GSS_Init_sec_context call. Upon receiving that, the
 * server calls the implementation equivalent of the abstract GSS_Accept_sec_context call, which
 * returns an auth_token and continue status in this example. Assume the following:
 *
 * 1) The client chooses the auth_context_id field in the sec_trailer sent with this PDU to be 1.
 *
 * 2) The client uses the RPC_C_AUTHN_LEVEL_PKT_PRIVACY authentication level and the
 *    Authentication Service (AS) NTLM.
 *
 * 3) The client sets the PFC_SUPPORT_HEADER_SIGN flag in the PDU header.
 */

int rpc_send_bind_pdu(rdpRpc* rpc)
{
	BYTE* buffer;
	UINT32 offset;
	UINT32 length;
	p_cont_elem_t* p_cont_elem;
	rpcconn_bind_hdr_t* bind_pdu;
	rdpSettings* settings = rpc->settings;

	rpc->ntlm = ntlm_new();

	DEBUG_RPC("Sending bind PDU");

	ntlm_client_init(rpc->ntlm, FALSE, settings->Username, settings->Domain, settings->Password);

	ntlm_authenticate(rpc->ntlm);

	bind_pdu = (rpcconn_bind_hdr_t*) malloc(sizeof(rpcconn_bind_hdr_t));
	ZeroMemory(bind_pdu, sizeof(rpcconn_bind_hdr_t));

	rpc_pdu_header_init(rpc, (rpcconn_hdr_t*) bind_pdu);

	bind_pdu->auth_length = rpc->ntlm->outputBuffer.cbBuffer;
	bind_pdu->auth_verifier.auth_value = rpc->ntlm->outputBuffer.pvBuffer;

	bind_pdu->ptype = PTYPE_BIND;
	bind_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG | PFC_SUPPORT_HEADER_SIGN | PFC_CONC_MPX;
	bind_pdu->call_id = 2;

	bind_pdu->max_xmit_frag = rpc->max_xmit_frag;
	bind_pdu->max_recv_frag = rpc->max_recv_frag;
	bind_pdu->assoc_group_id = 0;

	bind_pdu->p_context_elem.n_context_elem = 2;
	bind_pdu->p_context_elem.reserved = 0;
	bind_pdu->p_context_elem.reserved2 = 0;

	bind_pdu->p_context_elem.p_cont_elem = malloc(sizeof(p_cont_elem_t) * bind_pdu->p_context_elem.n_context_elem);

	p_cont_elem = &bind_pdu->p_context_elem.p_cont_elem[0];

	p_cont_elem->p_cont_id = 0;
	p_cont_elem->n_transfer_syn = 1;
	p_cont_elem->reserved = 0;
	CopyMemory(&(p_cont_elem->abstract_syntax.if_uuid), &TSGU_UUID, sizeof(p_uuid_t));
	p_cont_elem->abstract_syntax.if_version = TSGU_SYNTAX_IF_VERSION;

	p_cont_elem->transfer_syntaxes = malloc(sizeof(p_syntax_id_t));
	CopyMemory(&(p_cont_elem->transfer_syntaxes[0].if_uuid), &NDR_UUID, sizeof(p_uuid_t));
	p_cont_elem->transfer_syntaxes[0].if_version = NDR_SYNTAX_IF_VERSION;

	p_cont_elem = &bind_pdu->p_context_elem.p_cont_elem[1];

	p_cont_elem->p_cont_id = 1;
	p_cont_elem->n_transfer_syn = 1;
	p_cont_elem->reserved = 0;
	CopyMemory(&(p_cont_elem->abstract_syntax.if_uuid), &TSGU_UUID, sizeof(p_uuid_t));
	p_cont_elem->abstract_syntax.if_version = TSGU_SYNTAX_IF_VERSION;

	p_cont_elem->transfer_syntaxes = malloc(sizeof(p_syntax_id_t));
	CopyMemory(&(p_cont_elem->transfer_syntaxes[0].if_uuid), &BTFN_UUID, sizeof(p_uuid_t));
	p_cont_elem->transfer_syntaxes[0].if_version = BTFN_SYNTAX_IF_VERSION;

	offset = 116;
	bind_pdu->auth_verifier.auth_pad_length = rpc_offset_align(&offset, 4);

	bind_pdu->auth_verifier.auth_type = RPC_C_AUTHN_WINNT;
	bind_pdu->auth_verifier.auth_level = RPC_C_AUTHN_LEVEL_PKT_INTEGRITY;
	bind_pdu->auth_verifier.auth_reserved = 0x00;
	bind_pdu->auth_verifier.auth_context_id = 0x00000000;
	offset += (8 + bind_pdu->auth_length);

	bind_pdu->frag_length = offset;

	buffer = (BYTE*) malloc(bind_pdu->frag_length);

	CopyMemory(buffer, bind_pdu, 24);
	CopyMemory(&buffer[24], &bind_pdu->p_context_elem, 4);
	CopyMemory(&buffer[28], &bind_pdu->p_context_elem.p_cont_elem[0], 24);
	CopyMemory(&buffer[52], bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes, 20);
	CopyMemory(&buffer[72], &bind_pdu->p_context_elem.p_cont_elem[1], 24);
	CopyMemory(&buffer[96], bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes, 20);

	offset = 116;
	rpc_offset_pad(&offset, bind_pdu->auth_verifier.auth_pad_length);

	CopyMemory(&buffer[offset], &bind_pdu->auth_verifier.auth_type, 8);
	CopyMemory(&buffer[offset + 8], bind_pdu->auth_verifier.auth_value, bind_pdu->auth_length);
	offset += (8 + bind_pdu->auth_length);

	rpc_in_write(rpc, buffer, bind_pdu->frag_length);
	length = bind_pdu->frag_length;

	free(bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes);
	free(bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes);
	free(bind_pdu->p_context_elem.p_cont_elem);
	free(bind_pdu);
	free(buffer);

	return length;
}

/**
 * Maximum Transmit/Receive Fragment Size Negotiation
 *
 * The client determines, and then sends in the bind PDU, its desired maximum size for transmitting fragments,
 * and its desired maximum receive fragment size. Similarly, the server determines its desired maximum sizes
 * for transmitting and receiving fragments. Transmit and receive sizes may be different to help preserve buffering.
 * When the server receives the client’s values, it sets its operational transmit size to the minimum of the client’s
 * receive size (from the bind PDU) and its own desired transmit size. Then it sets its actual receive size to the
 * minimum of the client’s transmit size (from the bind) and its own desired receive size. The server then returns its
 * operational values in the bind_ack PDU. The client then sets its operational values from the received bind_ack PDU.
 * The received transmit size becomes the client’s receive size, and the received receive size becomes the client’s
 * transmit size. Either party may use receive buffers larger than negotiated — although this will not provide any
 * advantage — but may not transmit larger fragments than negotiated.
 */

/**
 *
 * SECURE_BIND_ACK: RPC bind_ack PDU with sec_trailer and auth_token. The PFC_SUPPORT_HEADER_SIGN
 * flag in the PDU header is also set in this example. Auth_token is generated by the server in the
 * previous step. Upon receiving that PDU, the client calls the implementation equivalent of the
 * abstract GSS_Init_sec_context call, which returns an auth_token and continue status in this example.
 */

int rpc_recv_bind_ack_pdu(rdpRpc* rpc)
{
	RPC_PDU* pdu;
	BYTE* auth_data;
	rpcconn_hdr_t* header;

	pdu = rpc_recv_pdu(rpc);

	if (!pdu)
		return -1;

	header = (rpcconn_hdr_t*) pdu->Buffer;

	rpc->max_recv_frag = header->bind_ack.max_xmit_frag;
	rpc->max_xmit_frag = header->bind_ack.max_recv_frag;

	rpc->ntlm->inputBuffer.cbBuffer = header->common.auth_length;
	rpc->ntlm->inputBuffer.pvBuffer = malloc(header->common.auth_length);

	auth_data = pdu->Buffer + (header->common.frag_length - header->common.auth_length);
	CopyMemory(rpc->ntlm->inputBuffer.pvBuffer, auth_data, header->common.auth_length);

	ntlm_authenticate(rpc->ntlm);

	return pdu->Length;
}

/**
 * RPC_AUTH_3: The client knows that this is an NTLM that uses three legs. It sends an rpc_auth_3
 * PDU with the auth_token obtained in the previous step. Upon receiving this PDU, the server calls
 * the implementation equivalent of the abstract GSS_Accept_sec_context call, which returns success
 * status in this example.
 */

int rpc_send_rpc_auth_3_pdu(rdpRpc* rpc)
{
	BYTE* buffer;
	UINT32 offset;
	UINT32 length;
	rpcconn_rpc_auth_3_hdr_t* auth_3_pdu;

	DEBUG_RPC("Sending auth_3 PDU");

	auth_3_pdu = (rpcconn_rpc_auth_3_hdr_t*) malloc(sizeof(rpcconn_rpc_auth_3_hdr_t));
	ZeroMemory(auth_3_pdu, sizeof(rpcconn_rpc_auth_3_hdr_t));

	rpc_pdu_header_init(rpc, (rpcconn_hdr_t*) auth_3_pdu);

	auth_3_pdu->auth_length = rpc->ntlm->outputBuffer.cbBuffer;
	auth_3_pdu->auth_verifier.auth_value = rpc->ntlm->outputBuffer.pvBuffer;

	auth_3_pdu->ptype = PTYPE_RPC_AUTH_3;
	auth_3_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG | PFC_CONC_MPX;
	auth_3_pdu->call_id = 2;

	offset = 20;

	auth_3_pdu->max_xmit_frag = rpc->max_xmit_frag;
	auth_3_pdu->max_recv_frag = rpc->max_recv_frag;

	offset += 4;
	auth_3_pdu->auth_verifier.auth_pad_length = rpc_offset_align(&offset, 4);

	auth_3_pdu->auth_verifier.auth_type = RPC_C_AUTHN_WINNT;
	auth_3_pdu->auth_verifier.auth_level = RPC_C_AUTHN_LEVEL_PKT_INTEGRITY;
	auth_3_pdu->auth_verifier.auth_reserved = 0x00;
	auth_3_pdu->auth_verifier.auth_context_id = 0x00000000;

	auth_3_pdu->frag_length = 20 + 4 +
			auth_3_pdu->auth_verifier.auth_pad_length + auth_3_pdu->auth_length + 8;

	buffer = (BYTE*) malloc(auth_3_pdu->frag_length);

	CopyMemory(buffer, auth_3_pdu, 24);

	offset = 24;
	rpc_offset_pad(&offset, auth_3_pdu->auth_verifier.auth_pad_length);

	CopyMemory(&buffer[offset], &auth_3_pdu->auth_verifier.auth_type, 8);
	CopyMemory(&buffer[offset + 8], auth_3_pdu->auth_verifier.auth_value, auth_3_pdu->auth_length);
	offset += (8 + auth_3_pdu->auth_length);

	rpc_in_write(rpc, buffer, auth_3_pdu->frag_length);
	length = auth_3_pdu->frag_length;

	free(auth_3_pdu);
	free(buffer);

	return length;
}

/**
 *           Secure Connection-Oriented RPC Packet Sequence
 *
 *     Client                                              Server
 *        |                                                   |
 *        |-------------------SECURE_BIND-------------------->|
 *        |                                                   |
 *        |<----------------SECURE_BIND_ACK-------------------|
 *        |                                                   |
 *        |--------------------RPC_AUTH_3-------------------->|
 *        |                                                   |
 *        |                                                   |
 *        |------------------REQUEST_PDU_#1------------------>|
 *        |------------------REQUEST_PDU_#2------------------>|
 *        |                                                   |
 *        |                        ...                        |
 *        |                                                   |
 *        |<-----------------RESPONSE_PDU_#1------------------|
 *        |<-----------------RESPONSE_PDU_#2------------------|
 *        |                                                   |
 *        |                        ...                        |
 */

int rpc_secure_bind(rdpRpc* rpc)
{
	if (rpc->State != RPC_CLIENT_STATE_ESTABLISHED)
	{
		printf("rpc_secure_bind: invalid state, expected RPC_CLIENT_STATE_ESTABLISHED\n");
		return -1;
	}

	if (rpc_send_bind_pdu(rpc) <= 0)
	{
		printf("rpc_send_bind_pdu error!\n");
		return -1;
	}

	rpc->State = RPC_CLIENT_STATE_WAIT_SECURE_BIND_ACK;

	if (rpc_recv_bind_ack_pdu(rpc) <= 0)
	{
		printf("rpc_recv_bind_ack_pdu error!\n");
		return -1;
	}

	if (rpc_send_rpc_auth_3_pdu(rpc) <= 0)
	{
		printf("rpc_send_rpc_auth_3 error!\n");
		return -1;
	}

	rpc->State = RPC_CLIENT_STATE_CONTEXT_NEGOTIATED;

	return 0;
}
