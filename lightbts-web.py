#!/usr/bin/python
# coding: utf-8

import argparse
import os
import platform
import getpass
import sys
import email
import mailbox
import smtplib
import sqlite3
import syslog
import string
import urlparse

version = '0.1'
copyright = 'LightBTS ' + version + ', copyright Ⓒ 2014 Guus Sliepen'
staticroot = '/'
root = '/'

statusnames = ['closed', 'open']
severitynames = ['wishlist', 'minor', 'normal', 'important', 'serious', 'grave', 'critical']


def init():
        global maildir, db
	maildir = mailbox.Maildir('btsmail')
	db = sqlite3.connect('bts.db')
	db.execute('PRAGMA foreign_key = on')

	db.execute('CREATE TABLE IF NOT EXISTS bugs (id INTEGER PRIMARY KEY AUTOINCREMENT, status INTEGER NOT NULL DEFAULT 1, severity INTEGER NOT NULL DEFAULT 2, title TEXT, owner TEXT, submitter TEXT)')
	db.execute('CREATE TABLE IF NOT EXISTS merges (a INTEGER, b INTEGER, PRIMARY KEY(a, b), FOREIGN KEY(a) REFERENCES bugs(id), FOREIGN KEY(b) REFERENCES bugs(id))')
	db.execute('CREATE TABLE IF NOT EXISTS messages (msgid PRIMARY KEY, key TEXT, bug INTEGER, spam INTEGER NOT NULL DEFAULT 0, FOREIGN KEY(bug) REFERENCES bugs(id))')
	db.execute('CREATE INDEX IF NOT EXISTS msgid_index ON messages (msgid)')
	db.execute('CREATE TABLE IF NOT EXISTS recipients (bug INTEGER, address TEXT, PRIMARY KEY(bug, address), FOREIGN KEY(bug) REFERENCES bugs(id))')
	db.execute('CREATE TABLE IF NOT EXISTS tags (bug INTEGER, tag TEXT, PRIMARY KEY(bug, tag), FOREIGN KEY(bug) REFERENCES bugs(id))')

        # Load templates
        global tmpl_main
        with open('template/main.html', 'r') as tmpl:
            tmpl_main = string.Template(tmpl.read())

        global tmpl_bugrow
        with open('template/bugrow.html', 'r') as tmpl:
            tmpl_bugrow = string.Template(tmpl.read())

        global tmpl_bug
        with open('template/bug.html', 'r') as tmpl:
            tmpl_bug = string.Template(tmpl.read())

        global tmpl_message
        with open('template/message.html', 'r') as tmpl:
            tmpl_message = string.Template(tmpl.read())

def exit():
	db.commit()
	db.close()

def do_bug(environ, start_response, query):
    init()
    id = query['bug']
    bug = db.execute('SELECT status, severity, title FROM bugs WHERE id=?', (id,))
    for i in bug:
        status = statusnames[i[0]]
        severity = severitynames[i[1]]
        title = str(i[2])
        break
    else:
        start_response('404 NOT FOUND', [('Content-Type', 'text/plain; charset=utf-8')])
        return 'Bug ' + query['bug'] + ' does not exist.\n'

    start_response('200 OK', [('Content-Type', 'text/html; charset=utf-8')])
    maildir = mailbox.Maildir('btsmail/' + id)
    msglist = ''
    msgs = db.execute('SELECT msgid, key FROM messages WHERE bug=?', (id,))
    for i in msgs:
        msg = email.Parser.Parser().parse(maildir.get_file(i[1]))
        body = msg.get_payload()
        if type(body) is list:
            body = '\n'.join(body)
        msglist += tmpl_message.substitute({'msgid': msg['Message-Id'], 'from': msg['From'], 'to': msg['To'], 'subject': msg['Subject'], 'date': msg['Date'], 'body': body})
    page = tmpl_bug.substitute({'id': id, 'submitter': '', 'date': '', 'status': status, 'severity': severity, 'title': title, 'msglist': msglist, 'root': root, 'copyright': copyright})
    exit()
    return page

def myapp(environ, start_response):
    query = dict(urlparse.parse_qsl(environ['QUERY_STRING']))
    if 'bug' in query:
        return do_bug(environ, start_response, query)

    start_response('200 OK', [('Content-Type', 'text/html; charset=utf-8')])

    init()
    bugs = db.execute('SELECT id, status, severity, title FROM bugs')
    buglist = ''
    for i in bugs:
        buglist += tmpl_bugrow.substitute({'id': i[0], 'status': statusnames[i[1]], 'severity': severitynames[i[2]], 'title': i[3]})
    page = tmpl_main.substitute({'buglist': str(buglist), 'copyright': copyright, 'root': staticroot})
    exit()

    return page

if __name__ == '__main__':
	# Initialize
	syslog.syslog("LightBTS web starting")

	parser = argparse.ArgumentParser(description='LightBTS web frontend.', epilog='Report bugs to guus@sliepen.org.')
	parser.add_argument('-d', '--data', metavar='DIR', help='directory where LightBTS stores its data')
	parser.add_argument('--version', action='version', version='LightBTS ' + version)

	args = parser.parse_args()

	if args.data:
	    os.chdir(args.data)
	else:
	    os.chdir(os.environ['HOME'])

	from flup.server.fcgi import WSGIServer
	WSGIServer(myapp).run()
