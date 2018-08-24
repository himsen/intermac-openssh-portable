#!/usr/bin/python

import subprocess
import os
import time
import math
import datetime

NUMBER_OF_SAMPLES = 3
FNAME_LOG_PREFIX = 'imopenssh'
FNAME_LOG_EXTENSION = 'json'
FNAME_SCP_SIZE = 1024 * 1024 # 500mb 1024 * 1024 * 500
FNAME_SCP = 'scp_copy'
SSH_DIR = os.getcwd()
USER = 'himsen'
DEST = 'localhost'
PORT = '22221'
ID_FILE = './id_rsa_im'
COMPRESSION_NO = 'Compression no'

# Ciphers to test
std_ciphers = [
	'aes128-ctr',
	'hmac-md5',
	'aes128-ctr',
	'hmac-md5-etm@openssh.com',
	'aes128-ctr',
	'umac-64-etm@openssh.com',
	'aes128-ctr',
	'hmac-sha1',
	'3des-cbc',
	'hmac-md5',
	'aes256-ctr',
	'hmac-sha2-512',
	'aes128-cbc',
	'hmac-sha1',
	'aes128-ctr',
	'hmac-ripemd160'
	]

auth_ciphers = [
	'chacha20-poly1305@openssh.com',
	'aes128-gcm@openssh.com'
	]

intermac_ciphers = [
	'im-aes128-gcm-127',
	'im-chacha-poly-127',
	'im-aes128-gcm-128',
	'im-chacha-poly-128',
	'im-aes128-gcm-255',
	'im-chacha-poly-255',	
	'im-aes128-gcm-256',
	'im-chacha-poly-256',
	'im-aes128-gcm-511',
	'im-chacha-poly-511',
	'im-aes128-gcm-512',
	'im-chacha-poly-512',
	'im-aes128-gcm-1023',
	'im-chacha-poly-1023',
	'im-aes128-gcm-1024',
	'im-chacha-poly-1024',
	'im-aes128-gcm-2047',
	'im-chacha-poly-2047',
	'im-aes128-gcm-2048',
	'im-chacha-poly-2048',
	'im-aes128-gcm-4095',
	'im-chacha-poly-4095',
	'im-aes128-gcm-4096',
	'im-chacha-poly-4096',
	'im-aes128-gcm-8191',
	'im-chacha-poly-8191',
	'im-aes128-gcm-8192',
	'im-chacha-poly-8192'
	]

# Execute SCP
def run_scp(cipher, mac, fd):

	# Construct terminal cmd
	if mac == None:
		cmd = '{}/scp -o "{}" -c {} -i {} -P {} {} {}@{}:'.format(
			SSH_DIR,
			COMPRESSION_NO,
			cipher,
			ID_FILE,
			PORT,
			FNAME_SCP,
			USER,
			DEST)
	else:
		cmd = '{}/scp -o "{}" -o "MACs {}" -c {} -i {} -P {} {} {}@{}:'.format(
			SSH_DIR,
			COMPRESSION_NO,
			mac,
			cipher,
			ID_FILE,
			PORT,
			FNAME_SCP,
			USER,
			DEST)


	# Execute
	ssh = subprocess.Popen(cmd, shell=True, stdout=fd)

	# Wait for connection to terminate
	ssh.wait()

	# Delay for a bit for processes to end on client and server
	# 50ms should be enough
	time.sleep(0.050)


def init_log_file(cipher, mac):

	with open(FNAME_LOG_PREFIX, "w+") as fd:
		if mac == None:
			fd.write('{}\n'.format(cipher))
		else:
			fd.write('{}+{}\n'.format(cipher, mac))
		fd.write('{}\n'.format(NUMBER_OF_SAMPLES))
		fd.write('{}\n'.format(str(datetime.datetime.now().strftime("%x"))))


def rename_log_file(cipher, mac):

	if mac == None:
		new_fname = '{}_{}.{}'.format(FNAME_LOG_PREFIX, cipher,
			FNAME_LOG_EXTENSION)
	else:
	 	new_fname = '{}_{}_{}.{}'.format(FNAME_LOG_PREFIX, cipher, mac,
			FNAME_LOG_EXTENSION)	
	print "Renaming file {} to {}".format(FNAME_LOG_PREFIX, new_fname)
	os.rename(FNAME_LOG_PREFIX, new_fname)


def delete_remote_test_file():

	# Construct terminal cmd
	cmd = 'ssh -i {} -p {} {}@{} "rm {}"'.format(
		ID_FILE,
		PORT,
		USER,
		DEST,
		FNAME_SCP)

	# Exeute
	ssh = subprocess.Popen(cmd, shell=True)

def run():
 
 	sample_progress = int((NUMBER_OF_SAMPLES + 9) / 10)

 	# Create test file
 	with open(FNAME_SCP, "wb") as fd:
 		fd.write('0' * FNAME_SCP_SIZE)
    	fd.close()

 	# Used to silence SSH ouput
 	with open(os.devnull, "w") as fd:

	 	print '**********Executing benchmarks'

		for cipher, mac in zip(std_ciphers[0::2],std_ciphers[1::2]):

			print '*****{}+{}'.format(cipher, mac)

			init_log_file(cipher, mac)

			i = 1
			for x in range(0, NUMBER_OF_SAMPLES):

				run_scp(cipher, mac, fd)

				if ((x + 1) % sample_progress == 0):
					print '{} samples collected'.format(sample_progress * i)
				i = i + 1

				delete_remote_test_file()

			rename_log_file(cipher, mac)

		for cipher_list in auth_ciphers, intermac_ciphers:
			for cipher in cipher_list:

				print '*****{}'.format(cipher)

				init_log_file(cipher, None)

				i = 1
				for x in range(0, NUMBER_OF_SAMPLES):

					run_scp(cipher, None, fd)

					if ((x + 1) % sample_progress == 0):
						print '{} samples collected'.format(sample_progress * i)
					i = i + 1

					delete_remote_test_file()

				rename_log_file(cipher, None)

		print '**********Finished benchmarks'

		fd.close()

	# Clean up
	os.remove(FNAME_SCP)

if __name__ == '__main__':

	print 'Number of samples for each cipher: {}'.format(NUMBER_OF_SAMPLES)
	print 'Ciphers selected:'
	for cipher, mac in zip(std_ciphers[0::2],std_ciphers[1::2]):
		print '{}+{}'.format(cipher, mac)

	for cipher_list in auth_ciphers, intermac_ciphers:
		for cipher in cipher_list:
			print cipher

	run()
