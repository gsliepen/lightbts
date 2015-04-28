#!/usr/bin/python

import argparse
import os
import platform
import getpass
import sys
import email
import smtplib

import lightbts

# Initialize

parser = argparse.ArgumentParser(description='Process incoming bug reports.', epilog='Report bugs to guus@sliepen.org.')
parser.add_argument('-a', '--admin', metavar='ADDRESS', help='admin email address')
parser.add_argument('-d', '--data', metavar='DIR', help='directory where LightBTS stores its data')
parser.add_argument('-f', '--from', metavar='ADDRESS', help='email address of this instance of LightBTS', dest='myaddress')
parser.add_argument('-n', '--name', metavar='NAME', help='name for this instance of LightBTS', default='LightBTS')
parser.add_argument('--version', action='version', version='LightBTS ' + lightbts.version)

args = parser.parse_args()

if args.myaddress:
    myaddress = args.myaddress
else:
    myaddress = getpass.getuser() + '@' + platform.node()

admin = args.admin

if args.data:
    lightbts.init(args.data)
else:
    lightbts.init(os.environ['HOME'])

msg = email.message_from_file(sys.stdin)
(bug, new) = lightbts.import_email(msg)

if new:
    subject = 'Bug#' + str(bug.id) + ': ' + msg['Subject']
    reply = email.MIMEText.MIMEText("Thank you for reporting a bug, which has been assigned number " + str(bug.id))
else:
    subject = msg['Subject']
    reply = email.MIMEText.MIMEText("Thank you for reporting additional information for bug number " + str(bug.id))

reply['From'] = args.name + ' <' + myaddress + '>'
reply['To'] = msg['From']
reply['Subject'] = subject

reply['Message-Id'] = email.utils.make_msgid('LightBTS')
reply['In-Reply-To'] = id

smtp = smtplib.SMTP('xar', 25)
smtp.sendmail(reply['From'], reply['To'], reply.as_string())

lightbts.record_msgid(bug.id, email.utils.unquote(reply['Message-Id']))

# Send a copy to the admin

if admin:
    msg.replace_header('Subject', subject)
    msg['Reply-To'] = myaddress
    smtp.sendmail(msg['From'], admin, msg.as_string())

db.commit()
db.close()
