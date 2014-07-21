#!/usr/bin/python

import argparse
import os
import platform
import getpass
import sys
import email
import mailbox
import smtplib
import sqlite3

version = '0.1'

# Initialize

parser = argparse.ArgumentParser(description='Process incoming bug reports.', epilog='Report bugs to guus@sliepen.org.')
parser.add_argument('-a', '--admin', metavar='ADDRESS', help='admin email address')
parser.add_argument('-d', '--data', metavar='DIR', help='directory where LightBTS stores its data')
parser.add_argument('-f', '--from', metavar='ADDRESS', help='email address of this instance of LightBTS', dest='myaddress')
parser.add_argument('-n', '--name', metavar='NAME', help='name for this instance of LightBTS', default='LightBTS')
parser.add_argument('--version', action='version', version='LightBTS ' + version)

args = parser.parse_args()

print args

if args.myaddress:
    myaddress = args.myaddress
else:
    myaddress = getpass.getuser() + '@' + platform.node()

admin = args.admin

if args.data:
    os.chdir(args.data)
else:
    os.chdir(os.environ['HOME'])

msg = email.message_from_file(sys.stdin)
maildir = mailbox.Maildir('btsmail')
db = sqlite3.connect('bts.db')

db.execute('''CREATE TABLE IF NOT EXISTS bugs (id INTEGER PRIMARY KEY AUTOINCREMENT, subject TEXT)''')
db.execute('''CREATE TABLE IF NOT EXISTS messages (key TEXT PRIMARY KEY, msgid TEXT UNIQUE, bug INTEGER)''')
db.execute('''CREATE INDEX IF NOT EXISTS msgid_index ON messages (msgid)''')
db.execute('''CREATE TABLE IF NOT EXISTS recipients (bug INTEGER, address TEXT, PRIMARY KEY(bug, address))''')

# Handle missing Message-Id

if not msg['Message-Id']:
	msg['Message-Id'] = email.utils.make_msgid('LightBTS')

# Save message to new

id = msg['Message-Id']
parent = msg['In-Reply-To']
subject = msg['Subject']
key = maildir.add(msg)

# Store the message in the database

try:
    db.execute("INSERT INTO messages (key, msgid, bug) values (?,?,?)", (key, id, 0));
except sqlite3.IntegrityError:
    print "Duplicate message!"
    sys.exit(1)

# Can we match the message to an existing bug?

bug = 0
new = False

matches = db.execute("SELECT bug FROM messages WHERE msgid=?", (parent,))
for i in matches:
    if i[0]:
        bug = i[0]

# Try finding one with a similar subject

if not bug:
	db.execute("SELECT id FROM bugs WHERE subject LIKE ?", (subject,))
	for i in matches:
	    if i[0]:
		bug = i[0]

if not bug:
    bug = db.execute("INSERT INTO bugs (subject) VALUES (?)", (subject,)).lastrowid
    new = True

db.execute("UPDATE messages SET bug=? WHERE key=?", (bug, key))

db.execute("INSERT OR IGNORE INTO recipients (bug, address) VALUES (?, ?)", (bug, msg['From']))

# Move the message to the apriopriate folder

maildir.close()
maildir = mailbox.Maildir('btsmail/' + str(bug))
movefrom = 'btsmail/new/' + key
moveto = 'btsmail/' + str(bug) + '/cur/' + key
os.rename(movefrom, moveto)

# Send a reply to the sender

if new:
    reply = email.MIMEText.MIMEText("Thank you for reporting a bug, which has been assigned number " + str(bug))
    subject = 'Bug#' + str(bug) + ': ' + subject
else:
    reply = email.MIMEText.MIMEText("Thank you for reporting additional information for bug number " + str(bug))

reply['From'] = args.name + ' <' + myaddress + '>'
reply['To'] = msg['From']
reply['Subject'] = subject

reply['Message-Id'] = email.utils.make_msgid('LightBTS')
reply['In-Reply-To'] = id

smtp = smtplib.SMTP('xar', 25)
smtp.sendmail(reply['From'], reply['To'], reply.as_string())

db.execute("INSERT INTO messages (msgid, bug) values (?,?)", (reply['Message-Id'], bug));

# Send a copy to the admin

if admin:
    msg.replace_header('Subject', subject)
    msg['Reply-To'] = myaddress
    smtp.sendmail(msg['From'], admin, msg.as_string())

db.commit()
db.close()
