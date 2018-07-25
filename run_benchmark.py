#!/usr/bin/python

import subprocess
import os
import time
import math
import datetime

NUMBER_OF_SAMPLES = 1
FNAME_LOG_PREFIX = 'imopenssh'
FNAME_LOG_EXTENSION = 'json'
FNAME_SCP_SIZE = 1024 * 1024 * 1 # 500mb
FNAME_SCP = 'scp_copy'
SSH_DIR = os.getcwd()
USER = 'himsen'
DEST = 'localhost'
PORT = '22221'
ID_FILE = './id_rsa_im'
COMPRESSION_NO = 'Compression no'

# Ciphers to test
std_ciphers = ['aes128-ctr',
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
	'es128-cbc',
	'hmac-sha1',
	'aes128-ctr',
	'hmac-ripemd160'
	]

auth_ciphers = ['chacha20-poly1305@openssh.com',
	'aes128-gcm@openssh.com'
	]

intermac_ciphers = ['im-aes128-gcm-128',
	'im-chacha-poly-128',
	'im-aes128-gcm-256',
	'im-chacha-poly-256',
	'im-aes128-gcm-512',
	'im-chacha-poly-512',
	'im-aes128-gcm-1024',
	'im-chacha-poly-1024',
	'im-aes128-gcm-2048',
	'im-chacha-poly-2048',
	'im-aes128-gcm-4096',
	'im-chacha-poly-4096'
	]

# Execute SCP
def run_scp(cipher, fd):

	# Construct terminal cmd
	cmd = '{}/scp -o "{}" -c {} -i {} -P {} {} {}@{}:'.format(
		SSH_DIR,
		COMPRESSION_NO,
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


def init_log_files():

	for alg_name_list in std_ciphers, auth_ciphers, intermac_ciphers:
		for alg_name in alg_name_list:
			fname = './{}_{}.{}'.format(FNAME_LOG_PREFIX, alg_name,
				FNAME_LOG_EXTENSION)
			with open(fname, "w+") as fd:
				fd.write('{\n')
				fd.write('"cipher":{},\n'.format(alg_name))
				fd.write('"number_of_samples":{},\n'.format(NUMBER_OF_SAMPLES))

def rename_log_files():

	t = int(time.time())

	for alg_name_list in std_ciphers, auth_cipher, intermac_ciphers:
		for alg_name in alg_name_list:
			fname = '{}_{}.{}'.format(FNAME_LOG_PREFIX, alg_name,
				FNAME_LOG_EXTENSION)
			with open(fname, "w+") as fd:
				fd.write('"date":{}'.format(str(datetime.datetime.new())))
				fd.write('}')
			new_fname = '{}_{}_{}.{}'.format(t, FNAME_LOG_PREFIX, alg_name,
				FNAME_LOG_EXTENSION)
			print "Renaming file {} to {}".format(fname, new_fname)
			os.rename(fname, new_fname)

def delete_remote_test_file():

	# Construct terminal cmd
	cmd = '{}/ssh -i {} -p {} {} "rm {}"'.format(
		SSH_DIR,
		ID_FILE,
		PORT,
		DEST,
		FNAME_SCP)

	# Exeute
	ssh = subprocess.Popen(cmd, shell=True, stdout=fd)

def run():
 
 	sample_progress = int((NUMBER_OF_SAMPLES + 9) / 10)

 	# Create test file
 	with open(FNAME_SCP, "wb") as fd:
 		fd.write('0' * FNAME_SCP_SIZE)
    	fd.close()

 	# Used to silence SSH ouput
 	with open(os.devnull, "w") as fd:

 		init_log_files()

	 	print '**********Executing benchmarks'

		for cipher_list in std_ciphers, auth_ciphers, intermac_ciphers:
			for cipher in cipher_list:

				print '*****{}'.format(cipher)

				i = 1
				for x in range(0, NUMBER_OF_SAMPLES):

					run_scp(cipher, fd)

					if ((x + 1) % sample_progress == 0):
						print '{} samples collected'.format(sample_progress * i)
					i = i + 1

		print '**********Finished benchmarks'

		fd.close()

		rename_log_files()

	# Clean up
	os.remove(FNAME_SCP)
	delete_remote_test_file()

if __name__ == '__main__':

	print 'Number of samples for each cipher: {}'.format(NUMBER_OF_SAMPLES)
	print 'Ciphers selected:'
	for cipher_list in std_ciphers, auth_ciphers, intermac_ciphers:
		for cipher in cipher_list:
			print cipher

	run()
