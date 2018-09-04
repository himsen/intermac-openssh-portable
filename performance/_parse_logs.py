#!/usr/bin/python2

import os
import numpy as np
import matplotlib.pyplot as plt
import statistics

# Size of file copied as part of benchmark {1,50,500}.mb
FILE_SIZE = 100

# Relative path to log directory
# 1mb
#LOG_DIR = './logs_1'
# 10mb
#LOG_DIR = './logs_10_3_region'
# 50mb
#LOG_DIR = './logs_50'
# 50mb for CT size
#LOG_DIR = './logs_50_ct'
# 100 mb (region)
LOG_DIR = './logs_100_25_region'
# 100mb
#LOG_DIR = './logs_100_5'
# 100mb (local)
#LOG_DIR = './logs_100_5_local'
# 100mb
#LOG_DIR = './logs_100_10_region'
# 500mb
#LOG_DIR = './logs_500_test'
# 500 mb
#LOG_DIR = './logs_500_5_local'


LOG_PREFIX = 'imopenssh_'
LOG_PREFIX_LEN = 10

# Header size in a log file
HEADER_SIZE = 3

NUMBER_OF_CHUNK_LENGTHS = 14
chunk_lengths = [
	127,
	128,
	255,
	256,
	511,
	512,
	1023,
	1024,
	2047,
	2048,
	4095,
	4096,
	8191,
	8192]

# Ciphers tested
number_of_std_ciphers = 16

std_ciphers = [
	'aes128-ctr+hmac-md5',
	'aes128-ctr+hmac-md5-etm@openssh.com',
	'aes128-ctr+umac-64-etm@openssh.com',
	'aes128-ctr+hmac-sha1',
	'3des-cbc+hmac-md5',
	'aes256-ctr+hmac-sha2-512',
	'aes128-cbc+hmac-sha1',
	'aes128-ctr+hmac-ripemd160'
	]

std_ciphers_labels = [
	'aes128-ctr+hmac-md5',
	'aes128-ctr+hmac-md5-etm@',
	'aes128-ctr+umac-64-etm@',
	'aes128-ctr+hmac-sha1',
	'3des-cbc+hmac-md5',
	'aes256-ctr+hmac-sha2-512',
	'aes128-cbc+hmac-sha1',
	'aes128-ctr+hmac-ripemd160'
	]

number_of_auth_ciphers = 2
auth_ciphers = [
	'chacha20-poly1305@openssh.com',
	'aes128-gcm@openssh.com'
	]

auth_ciphers_labels = [
	'chacha20-poly1305@',
	'aes128-gcm@'
	]

number_of_intermac_ciphers = 28
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

labels_im = []
labels_std_auth = []

time_im = []
bytes_sent_ct_im = []
bytes_sent_raw_im = []
time_std_auth = []
bytes_sent_ct_std_auth = []
bytes_sent_raw_std_auth = []

def compute_time_median(median_list, data):

	data_mb_s = []

	for x in map(float, data):
		if x != 0:
			data_mb_s.append( FILE_SIZE / x )

	median_list.append(np.median(data_mb_s))

def compute_bytes_average(avg_list, data):

	data_mb = []
	parsed_data = []
	MB = 0.000001

	data_mb = [ x * MB for x in map(int, data) ]
	avg_list.append(np.average(map(float, data_mb)))

def parse_data(data):

	time_list = []
	ct_list = []
	raw_list = []

	for i in range(0, len(data) - 1, 3):
		time_list.append(data[i][11:])
		ct_list.append(data[i+1][22:])
		raw_list.append(data[i+2][16:])

	return time_list, ct_list, raw_list

def parse_logs():

	cipher = None
	stat_size = 0
	time_list = []
	ct_list = []
	raw_list = []
	label_index = 0

	for c in intermac_ciphers:

		with open(os.path.join(LOG_DIR, '{}{}'.format(LOG_PREFIX, c)), 'r') as fd:

			# Split by newline
			log = fd.read().split('\n')

			# Get header info
			# (cipher, sample size, date of benchmark)
			cipher = log[0]
			stat_size = int(log[1])
			date = log[2]

			print 'Cipher: {}'.format(cipher)

			# Parse sample data
			time_list, ct_list, raw_list = parse_data(log[HEADER_SIZE:])

			if (cipher in intermac_ciphers):

				labels_im.append(cipher)
				compute_time_median(time_im, time_list)
				compute_bytes_average(bytes_sent_ct_im, ct_list)
				compute_bytes_average(bytes_sent_raw_im, raw_list)

	label_index = 0

	for c in std_ciphers:

		with open(os.path.join(LOG_DIR, '{}{}'.format(LOG_PREFIX, c)), 'r') as fd:

			# Split by newline
			log = fd.read().split('\n')

			# Get header info
			# (cipher, sample size, date of benchmark)
			cipher = log[0]
			stat_size = int(log[1])
			date = log[2]

			print 'Cipher: {}'.format(cipher)

			# Parse sample data
			time_list, ct_list, raw_list = parse_data(log[HEADER_SIZE:])

			# Branch depending on type of cipher
			if cipher in std_ciphers:

				labels_std_auth.append(std_ciphers_labels[label_index])
				compute_time_median(time_std_auth, time_list)
				compute_bytes_average(bytes_sent_ct_std_auth, ct_list)
				compute_bytes_average(bytes_sent_raw_std_auth, raw_list)
				label_index = label_index + 1

	label_index = 0

	for c in auth_ciphers:

		with open(os.path.join(LOG_DIR, '{}{}'.format(LOG_PREFIX, c)), 'r') as fd:

			# Split by newline
			log = fd.read().split('\n')

			# Get header info
			# (cipher, sample size, date of benchmark)
			cipher = log[0]
			stat_size = int(log[1])
			date = log[2]

			print 'Cipher: {}'.format(cipher)

			# Parse sample data
			time_list, ct_list, raw_list = parse_data(log[HEADER_SIZE:])

			# Branch depending on type of cipher
			if cipher in auth_ciphers:

				labels_std_auth.append(auth_ciphers_labels[label_index])
				compute_time_median(time_std_auth, time_list)
				compute_bytes_average(bytes_sent_ct_std_auth, ct_list)
				compute_bytes_average(bytes_sent_raw_std_auth, raw_list)
				label_index = label_index + 1

def draw_graph(ax, labels, data, title, xlabel, ylimit):

	# Max x-label 1mb
	#max_x_label = 10000
	# Max x-label 100mb
	#max_x_label = 15
	#max_x_label = 150
	#max_x_label = 8.5
	#max_x_label = 135
	max_x_label = 105

	y = np.arange(len(labels) * 2, step=2)
	height = 1.2

	rec = ax.barh(y, data, height, align='center', color='red')
	ax.set_title(title)
	ax.set_yticks(y)
	ax.set_yticklabels(labels)
	#ax.invert_yaxis()
	ax.set_xlabel(xlabel)
	ax.set_xlim(0, max_x_label)
	ax.set_ylim(-1.5, ylimit - 0.5)
	
	ax.grid(color='green', linestyle='-')

	for r in rec:
		w = r.get_width()
		if not w == 0:
			ax.text(10, r.get_y() + 0.5, '{}'.format(w), color='blue', fontweight='bold')

def do_graphs():

	chart_title_im = ''
	chart_title_std_auth = ''

	#fig, axes = plt.subplots(nrows=3, ncols=2, figsize=(10,10))
	fig, axes = plt.subplots(nrows=1, ncols=2, figsize=(10,10))

	#ax1, ax2, ax3, ax4, ax5, ax6 = axes.flatten()
	ax1, ax2 = axes.flatten()

	# Time
	#draw_graph(ax1, labels_im, time_im, chart_title_im, 'MB/s', 56)
	#draw_graph(ax2, labels_std_auth, time_std_auth, chart_title_std_auth, 'MB/s', 20)

	# Bytes sent ciphertext
	#draw_graph(ax1, labels_im, bytes_sent_ct_im, chart_title_im, 'MB', 56)
	#draw_graph(ax2, labels_std_auth, bytes_sent_ct_std_auth, chart_title_std_auth, 'MB', 20)

	# Bytes sent raw
	draw_graph(ax1, labels_im, bytes_sent_raw_im, chart_title_im, 'MB', 56)
	draw_graph(ax2, labels_std_auth, bytes_sent_raw_std_auth, chart_title_std_auth, 'MB', 20)

	plt.tight_layout()
	plt.show()

if __name__ == '__main__':

	parse_logs()

	print 'Labels IM ciphers:\n{}'.format(labels_im)
	print 'Medians IM ciphers:\n{}'.format(time_im)
	print 'Bytes sent CT IM ciphers:\n{}'.format(bytes_sent_ct_im)
	print 'Bytes sent raw IM ciphers:\n{}'.format(bytes_sent_raw_im)
	print 'Labels STD/AUTH ciphers:\n{}'.format(labels_std_auth)
	print 'Medians STD_AUTH ciphers:\n{}'.format(time_std_auth)
	print 'Bytes sent CT STD/AUTH ciphers:\n{}'.format(bytes_sent_ct_std_auth)
	print 'Bytes sent raw STD/AUTH ciphers:\n{}'.format(bytes_sent_raw_std_auth)

	do_graphs()
