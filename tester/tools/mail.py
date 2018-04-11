#!/usr/bin/python

#Start off by creating a default template
#Read the files to get all the information needed
#Generate an mbox of all the emails
#use bash script to send the mbox using sendmail

import sys, os, smtplib
from email.MIMEMultipart import MIMEMultipart
from email.MIMEBase import MIMEBase
from email.MIMEText import MIMEText
from email.Utils import COMMASPACE, formatdate
from email import Encoders
from subprocess import Popen, PIPE
#We will remove these once everything is tested
import mailbox
import email.utils
import os.path
from optparse import OptionParser

Total = 0
check = True
mailfile = "../emails.txt"
designfile = "design-marks.csv"

def generateSalutation(utorid):
	d_line = 'Dear'
	for i in utorid:
		d_line+= " " + i

	d_line += '\n\n'
	return d_line

def storeResults(utorid, mark, total):
	results = open('results.csv', 'a')
	for i in utorid:
		results.write(i + ',' + total + ',' + mark + '\n')
	results.close()

def generateBody(grp, asst, marks, utorid):
	global check
	global Total
	b_line ='Your submission for Assignment ' + str(asst) + ' has been automarked\n\n'
	total = 0
	mark = 0
	for i in marks:
		total += int(i['total'])
		mark += int(i['mark'])

	if Total == 0:
		Total = total
	elif total != total:
		check = False

	b_line += 'You have scored ' + str(mark) + ' out of a possible '+ str(total) + ' marks.\n\n'
	b_line += 'You have scored: \n\n'

	for i in marks:
		if int(i['total']) == 0:
			continue
		b_line += i['name'] + ': ' + str(i['mark']) + ' out of ' + str(i['total']) + '\n'

	b_line += '\n'
	storeResults(utorid, str(mark), str(total))

	return b_line

def generateFail(grp, asst, mark):
	b_line ='Your submission for Assignment ' + str(asst) + ' has been automarked\n\n'
	b_line += 'Your submission failed. Please check attachment for the reasons\n\n'
	if mark:
		b_line += 'Your design score is ' + str(mark['mark']) + ' out of ' + str(mark['total']) + '\n\n'
		storeResults(grp, str(mark['mark']), str(mark['total']))
	return b_line

def generateBye(TA):
	l_line = 'Please contact ' + TA + ' if you have further questions regarding your marks.\n\n'
	l_line += 'Best of luck for the next assignment.\n\n'
	l_line += 'ECE 344 TAs\n'
	return l_line

def parseMarkFile(grp):
	mark = []
	try:
		filename = 'os161-mark-'+ grp +'.txt'
		f = open(filename, 'r')
		for l in f.readlines():
			line = l.split(',')
			mark.append({'name': line[0], 'total': line[1], 'mark': line[2][:-1]})
		f.close()
	except IOError:
		print 'Failed to find the mark file for group ' + grp + '. Assignment failed to build.'
	return mark

def parseDesignMark(grp):
	mark = {}
	global designfile
	filename = designfile
	try:
		f = open(filename, 'r')
		for l in f.readlines():
			if grp in l:
				mark['name'] = 'Design'
				line = l.split(',')
				mark['total'] = line[1]
				mark['mark'] = line[2][:-1]
	except IOError:
		print 'No design in this assignment'
	return mark


def parseEmailFile(grp):
	#filename = 'emails.txt'
	global mailfile
	f = open(mailfile, 'r')
	utorid = []
	email = []
	for line in f.readlines():
		if grp in line:
			l = line.split(',')
			i = 0
			for value in l:
				if i == 0:
					i += 1
					continue
				elif i % 2 == 1:
					#Must be utorid
					utorid.append(value.strip())
				else:
					#Must be email id
					email.append(value.strip())
				i += 1
	f.close()
	return (utorid, email)

def generateMail(email, text, asst, grp, files, TA):
	msg = MIMEMultipart()
	msg['From'] = TA
	msg['To'] = COMMASPACE.join(email)
	#msg['Date'] = format(localtime=True)
	msg['Subject'] = 'Automarker results for Assignment ' + asst
	msg.attach(MIMEText(text))

	for f in files:
		part = MIMEBase('application', "octet-stream")
		part.set_payload( open(f,"rb").read() )
		Encoders.encode_base64(part)
		part.add_header('Content-Disposition', 'attachment; filename="%s"' % os.path.basename(f))
		msg.attach(part)

	return msg

def generateEmail(grp, asst, TA):
	(utorid, email) = parseEmailFile(grp)
	mark = parseMarkFile(grp)
	design = parseDesignMark(grp)
	if not mark:
		files = ["os161-marker-" + grp + ".log"]
		body = generateFail(grp, asst, design)
	else:
		if design:
			mark.append(design)
		files = ["os161-" + grp + ".log", "os161-marker-" + grp + ".log", "os161-tester-" + grp + ".log"]
		body = generateBody(grp, asst, mark, utorid)
	hello = generateSalutation(utorid)
	bye = generateBye(TA)
	email.append("dhaval@eecg.toronto.edu")
	email.append(TA)
	message = hello + body + bye
	return generateMail(email, message, asst, grp, files, TA)

def generateMbox(asst,start, stop, TA):
	mbox = mailbox.mbox('test.mbox')
	mbox.lock()
	try:
		for i in range(start, stop+1):
			grp = u'%03d' % i
			email = generateEmail(grp, asst, TA)
			#The group doesn't  exist
			if email is None:
				continue
			print "Email for os-" + grp + " generated"
			mbox.add(email)
			mbox.flush()
			print "Email for os-" + grp + " added"
	finally:
		mbox.unlock()
	return

def sendEmail(asst, start, stop, TA):
	for i in range(start, stop+1):
		p = Popen(["/usr/sbin/sendmail", "-t"], stdin=PIPE)
		grp = u'%03d' % i
		email = generateEmail(grp, asst, TA)
		if email is None:
			continue
		print "Email for os-" + grp + " generated"
		p.communicate(email.as_string())
		print "Email for os-" + grp + " sent"


def main():
	global check
	global Total
	global mailfile
	global designfile
	parser = OptionParser()
	parser.add_option("-t", "--TA", action = "store", type = "string", dest = "TA", help = "Set the TA for the current assignment. This information is used to populate the email address in the from field")
	parser.add_option("-s", "--start", action = "store", type = "int", dest = "start", default = 1, help = "Set the group from which the mailer runs")
	parser.add_option("-e", "--end", action = "store", type = "int", dest = "end", default = 39, help = "Set the group till which the mailer runs")
	parser.add_option("-r", "--real", action = "store_false", dest = "dry", default = True, help = "Set this option once you are convinced that all the mboxes look right. The mailer will run and actually send the email then")
	parser.add_option("-a", "--assignment", action = "store", type = "string", dest = "asst", help = "Set the assignment number")
	parser.add_option("-m", "--email-file", action = "store", type = "string", dest = "email", help = "Set the file having the email ids")
	parser.add_option("-d", "--design-file", action = "store", type = "string", dest = "design", help = "Set the file having the design marks")

	(options, args) = parser.parse_args(sys.argv)
	if options.TA is None:
		print "Please provide TA details using the -t/--TA option"
		exit()
	if options.asst is None:
		print "Please provide the assignment number using -a/--assignment option"
		exit()
	if options.design is not None:
		designfile = options.design
	if options.email is not None:
		mailfile = options.email
	check = True
	Total = 0
	asst = options.asst
	generateMbox(asst, options.start, options.end, options.TA)
	if check is False:
		print "Totals don't match. Please check before sending emails"
		exit()
	if options.dry is False:
		print("Sending Emails\n")
		sendEmail(asst, options.start, options.end, options.TA)

if __name__ == "__main__":
	main()
