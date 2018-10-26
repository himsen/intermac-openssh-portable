#!/usr/bin/python2.7

import os
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec

# Size of file copied as part of benchmark {1,50,500}.mb
FILE_SIZE = 50

# Relative path to log directory
# 1mb
#LOG_DIR = './logs_1'
# 10mb
#LOG_DIR = './logs_10_3_region'
# 50mb
LOG_DIR = './logs_50'
# 100mb (region)
#LOG_DIR = './logs_100_10_region'
# 100mb (region)
#LOG_DIR = './logs_100_25_region'
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

std_ciphers_grab = [
	'aes128-ctr_hmac-md5',
	'aes128-ctr_hmac-md5-etm@openssh.com',
	'aes128-ctr_umac-64-etm@openssh.com',
	'aes128-ctr_hmac-sha1',
	'3des-cbc_hmac-md5',
	'aes256-ctr_hmac-sha2-512',
	'aes128-cbc_hmac-sha1',
	'aes128-ctr_hmac-ripemd160'
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
	'im-aes128-gcm-128',
	'im-chacha-poly-128',	
	'im-aes128-gcm-512',
	'im-chacha-poly-512',
	'im-aes128-gcm-1024',
	'im-chacha-poly-1024',
	'im-aes128-gcm-2048',
	'im-chacha-poly-2048',
	'im-aes128-gcm-8192',
	'im-chacha-poly-8192'
	]

number_of_ciphers = 13
ciphers = [
	'3des-cbc+hmac-md5',
	'chacha20-poly1305@openssh.com',
	'aes128-gcm@openssh.com',
	'im-aes128-gcm-128',
	'im-chacha-poly-128',	
	'im-aes128-gcm-512',
	'im-chacha-poly-512',
	'im-aes128-gcm-1024',
	'im-chacha-poly-1024',
	'im-aes128-gcm-2048',
	'im-chacha-poly-2048',
	'im-aes128-gcm-8192',
	'im-chacha-poly-8192'
	]

ciphers_label = [
	'3des-cbc+hmac-md5',
	'chacha20-poly1305@',
	'aes128-gcm@',
	'im-aes128-gcm-128',
	'im-chacha-poly-128',	
	'im-aes128-gcm-512',
	'im-chacha-poly-512',
	'im-aes128-gcm-1024',
	'im-chacha-poly-1024',
	'im-aes128-gcm-2048',
	'im-chacha-poly-2048',
	'im-aes128-gcm-8192',
	'im-chacha-poly-8192'
	]

ciphers_grab = [
	'3des-cbc_hmac-md5',
	'chacha20-poly1305@openssh.com',
	'aes128-gcm@openssh.com',
	'im-aes128-gcm-128',
	'im-chacha-poly-128',	
	'im-aes128-gcm-512',
	'im-chacha-poly-512',
	'im-aes128-gcm-1024',
	'im-chacha-poly-1024',
	'im-aes128-gcm-2048',
	'im-chacha-poly-2048',
	'im-aes128-gcm-8192',
	'im-chacha-poly-8192'
	]

labels = []

time = []
bytes_sent_raw = []

bytes_sent_ct = [
	52.505076799999998,
	52.488540799999996,
	52.48668399999999,
	59.635280999999999,
	59.703140999999995,
	55.020972600000007,
	55.396985799999996,
	54.972087000000002,
	55.484675399999993,
	56.660295999999995,
	56.919246999999999,
	72.262185200000005,
	71.61859960000001
]

def compute_time_median(median_list, data):

	data_mb_s = []

	for x in map(float, data):
		if x != 0:
			data_mb_s.append( FILE_SIZE / x )

	median_list.append(np.median(data_mb_s))

def compute_bytes_average(avg_list, data):

	data_mb = []
	MB = 0.000001

	data_mb = [ x * MB for x in map(int, data) ]
	avg_list.append(np.average(map(float, data_mb)))

def parse_data(data):

	time_list = []
	ct_list = []
	raw_list = []

	for i in range(0, len(data) - 1, 3):
		time_list.append(data[i])
		ct_list.append(data[i+1])
		raw_list.append(data[i+2])

	return time_list, ct_list, raw_list

def parse_logs():

	cipher = None
	stat_size = 0
	time_list = []
	ct_list = []
	raw_list = []
	label_index = 0

	label_index = 0

	for c in ciphers_grab:

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
			if cipher in ciphers:

				labels.append(ciphers_label[label_index])
				compute_time_median(time, time_list)
				compute_bytes_average(bytes_sent_raw, raw_list)
				label_index = label_index + 1

	label_index = 0

def draw_graph(ax, labels, data, title, xlabel, ylimit, x_label_if):

	# Max x-label 1mb
	#max_x_label = 10000
	# Max x-label 50mb
	#max_x_label = 300
	if (x_label_if == 1):
		max_x_label = 265
	elif (x_label_if == 2):
		max_x_label = 85

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
	
	ax.xaxis.grid(color='green', linestyle='-')

	#for r in rec:
	#	w = r.get_width()
	#	if not w == 0:
	#		ax.text(10, r.get_y() + 0.5, '{}'.format(w), color='blue', fontweight='bold')

def do_graphs():

	chart_title_im = ''
	chart_title_std_auth = ''

	#fig, axes = plt.subplots(nrows=3, ncols=2, figsize=(10,10))
	fig, axes = plt.subplots(nrows=1, ncols=2, figsize=(10,10))

	#ax1, ax2, ax3, ax4, ax5, ax6 = axes.flatten()
	ax1, ax2 = axes.flatten()

	# Time
	draw_graph(ax1, labels_im, time_im, chart_title_im, 'MB/s', 56)
	draw_graph(ax2, labels_std_auth, time_std_auth, chart_title_std_auth, 'MB/s', 20)

	# Bytes sent ciphertext
	#draw_graph(ax1, labels_im, bytes_sent_ct_im, chart_title_im, 'MB', 56)
	#draw_graph(ax2, labels_std_auth, bytes_sent_ct_std_auth, chart_title_std_auth, 'MB', 20)

	# Bytes sent raw
	#draw_graph(ax1, labels_im, bytes_sent_raw_im, chart_title_im, 'MB', 56)
	#draw_graph(ax2, labels_std_auth, bytes_sent_raw_std_auth, chart_title_std_auth, 'MB', 1200)

	plt.tight_layout()
	plt.show()

def do_graphs_grid():

	chart_title_throughput = 'Throughput'
	chart_title_ct = 'Total ciphertext volume'

	fig = plt.figure(figsize=(9,3.7))

	fig.suptitle('{}'.format('50mb SCP file transfer'), fontsize=15, x=0.59)

	gs = gridspec.GridSpec(1, 2, width_ratios=[1.3, 1])
	ax1 = plt.subplot(gs[0])
	ax2 = plt.subplot(gs[1])

	# Time
	draw_graph(ax1, labels, time, chart_title_throughput, 'MB/s', 26, 1)

	# Bytes sent ciphertext
	draw_graph(ax2, labels, bytes_sent_ct, chart_title_ct, 'MB', 26, 2)

	# Bytes sent raw
	#draw_graph(ax1, labels, bytes_sent_raw, chart_title, 'MB', 56)
	plt.tight_layout(pad=1, w_pad=1, h_pad=1.5, rect=[0, 0, 1, 0.97])
	#plt.tight_layout(pad=1, w_pad=1, h_pad=1.5)
	plt.show()

if __name__ == '__main__':

	parse_logs()

	print 'Labels ciphers:\n{}'.format(ciphers_label)
	print 'Medians ciphers:\n{}'.format(time)
	print 'Bytes sent CT ciphers:\n{}'.format(bytes_sent_ct)
	print 'Bytes sent raw ciphers:\n{}'.format(bytes_sent_raw)

	do_graphs_grid()
