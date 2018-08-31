#!/usr/bin/env bash

set -o pipefail

NUMBER_OF_SAMPLES=1
FNAME_LOG_PREFIX=imopenssh
DEST=
PORT=
USER=
ID_FILE=
SCP=$(pwd)/scp
# 100mb
FNAME_SCP_SIZE=1048576
FNAME_SCP='scp_copy'
DATE=`date +%Y-%m-%d:%H:%M:%S`

GREP_SCP='Bytes per second sent\|Bytes encrypted sent\|Bytes raw sent'

CIPHER_SUITES=("aes128-ctr" "hmac-md5" "aes128-ctr" "hmac-md5-etm@openssh.com" "aes128-ctr" "umac-64-etm@openssh.com" "aes128-ctr" 
	"hmac-sha1" "3des-cbc" "hmac-md5" "aes256-ctr" "hmac-sha2-512" "aes128-cbc" "hmac-sha1" "aes128-ctr" "hmac-ripemd160")
AUTH_CIPHER_SUITES=("chacha20-poly1305@openssh.com" "aes128-gcm@openssh.com")
INTERMAC_CIPHER_SUITES=("im-aes128-gcm-127" "im-chacha-poly-127" "im-aes128-gcm-128" "im-chacha-poly-128" 
	"im-aes128-gcm-255" "im-chacha-poly-255" "im-aes128-gcm-256" "im-chacha-poly-256" 
	"im-aes128-gcm-511" "im-chacha-poly-511" "im-aes128-gcm-512" "im-chacha-poly-512" 
	"im-aes128-gcm-1023" "im-chacha-poly-1023" "im-aes128-gcm-1024" "im-chacha-poly-1024" 
	"im-aes128-gcm-2047" "im-chacha-poly-2047" "im-aes128-gcm-2048" "im-chacha-poly-2048"
	"im-aes128-gcm-4095" "im-chacha-poly-4095" "im-aes128-gcm-4096" "im-chacha-poly-4096" 
	"im-aes128-gcm-8191" "im-chacha-poly-8191" "im-aes128-gcm-8192" "im-chacha-poly-8192")

rm_remote_data () {

	ssh -i $ID_FILE $USER@$DEST "rm $FNAME_SCP"

}

rm_local_data () {

	rm $FNAME_SCP

}

generate_test_data () {
	
	dd if=/dev/zero of=$FNAME_SCP bs=$FNAME_SCP_SIZE count=1 &> /dev/null

}

scp_cipher_mac () {

	CIPHER=$1
	MAC=$2
	echo "$CIPHER + $MAC"
	echo "$CIPHER+$MAC" >> $LOG_FILE_NAME
	$SCP -v -c $CIPHER -o "MACs $MAC" -o 'Compression no' -i $ID_FILE -P $PORT $FNAME_SCP $USER@$DEST: |& grep "$GREP_SCP" >> $FNAME_LOG_PREFIX

}

scp_auth_cipher () {

	AUTHCIPHER=$1
	echo "$AUTHCIPHER"
	echo "$AUTHCIPHER" >> $LOG_FILE_NAME
	$SCP -v -c $AUTHCIPHER -o 'Compression no' -i $ID_FILE -P $PORT $FNAME_SCP $USER@$DEST: |& grep "$GREP_SCP" >> $FNAME_LOG_PREFIX

}

echo ""
echo "-----SCP BENCHMARK START-----"
echo ""

echo "Constructing temp data file:"
echo "File size: $FNAME_SCP_SIZE"

generate_test_data

echo ""
echo "Executing SCP using cipher suite"
echo ""

# "normal" ciphers suites
for ((i=0; i<${#CIPHER_SUITES[@]}; i+=2));
do
	echo "${CIPHER_SUITES[i]}+${CIPHER_SUITES[i+1]}" >> $FNAME_LOG_PREFIX
	echo $NUMBER_OF_SAMPLES >> $FNAME_LOG_PREFIX
	echo $DATE >> $FNAME_LOG_PREFIX
	for ((j=0; j<$NUMBER_OF_SAMPLES; j++));
	do 
		scp_cipher_mac "${CIPHER_SUITES[i]}" "${CIPHER_SUITES[i+1]}"
	done
	FNAME_NEW_LOG="imopenssh_${CIPHER_SUITES[i]}+${CIPHER_SUITES[i+1]}"
	mv $FNAME_LOG_PREFIX FNAME_NEW_LOG
done

# AE cipher suites
for ((i=0; i<${#AUTH_CIPHER_SUITES[@]}; i+=1));
do
	echo "${AUTH_CIPHER_SUITES[i]}" >> $FNAME_LOG_PREFIX
	echo $NUMBER_OF_SAMPLES >> $FNAME_LOG_PREFIX
	echo $DATE >> $FNAME_LOG_PREFIX
	for ((j=0; j<$NUMBER_OF_SAMPLES; j++));
	do 
		scp_auth_cipher "${AUTH_CIPHER_SUITES[i]}"
	done
	FNAME_NEW_LOG="${AUTH_CIPHER_SUITES[i]}"
	mv $FNAME_LOG_PREFIX FNAME_NEW_LOG
done

# InterMAC cipher suites
for ((i=0; i<${#INTERMAC_CIPHER_SUITES[@]}; i+=1));
do
	echo "${INTERMAC_CIPHER_SUITES[i]}" >> $FNAME_LOG_PREFIX
	echo $NUMBER_OF_SAMPLES >> $FNAME_LOG_PREFIX
	echo $DATE >> $FNAME_LOG_PREFIX
	for ((j=0; j<$NUMBER_OF_SAMPLES; j++));
	do 
		scp_auth_cipher "${INTERMAC_CIPHER_SUITES[i]}"
	done
	FNAME_NEW_LOG="imopenssh_${INTERMAC_CIPHER_SUITES[i]}"
	mv $FNAME_LOG_PREFIX FNAME_NEW_LOG
done

rm_local_data
rm_remote_data

echo ""
echo "-----SCP BENCHMARK END-----"
echo ""
