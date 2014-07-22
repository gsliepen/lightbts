#!/usr/bin/python

import argparse
import os
import platform
import getpass
import sys
import smtplib
import sqlite3
import email
import mailbox

version = '0.1'

statusnames = ['closed', 'open']
severitynames = ['wishlist', 'minor', 'normal', 'important', 'serious', 'grave', 'critical']

def do_init(args):
    print 'LightBTS initialized.'

def do_list(args):
    cmd = 'SELECT id, status, severity, title FROM bugs WHERE 1'
    cmdargs = []
    status = 1
    severities = []
    tags = []
    for i in args.prop:
        if i == 'all':
            status = None
        elif i in statusnames:
            status = statusnames.index(i)
        elif i in severitynames:
            severities.append(severitynames.index(i))
        else:
            tags.append(i)

    if status is not None:
        cmd += ' AND status=?'
        cmdargs.append(status)
    
    if severities:
        cmd += ' AND (0'
        for i in severities:
            cmd += ' OR severity=?'
            cmdargs.append(i)
        cmd += ')'

    if tags:
        cmd += ' AND (0'
        for i in tags:
            cmd += ' OR EXISTS (SELECT 1 FROM tags WHERE bug=id AND tag=?)'
            cmdargs.append(i)
        cmd += ')'
            
    bugs = db.execute(cmd, tuple(cmdargs))
    for i in bugs:
        print "{:>6} {:6} {:9}  {:}".format(i[0], statusnames[i[1]], severitynames[i[2]], i[3])

def do_show(args):
    try:
        bug = int(args.id)
    except:
        bug = 0
    if str(bug) != args.id:
        bug = 0
        matches = db.execute('SELECT bug, key FROM messages WHERE msgid=?', (args.id,))
        for i in matches:
            if i[0]:
                bug = i[0]
                key = i[1]
        if not bug:
            print 'Could not find Message-Id ' + args.id
            return
    
    matches = db.execute('SELECT status, severity, title FROM bugs WHERE id=?', (bug,))
    for i in matches:
        status = statusnames[i[0]]
        severity = severitynames[i[1]]
        title = i[2]
        break
    else:
        print 'Could not find bug #' + str(bug)
        return

    print 'Bug#' + str(bug) + ': ' + title
    print 'Status: ' + status
    print 'Severity: ' + severity
    tagtext = ''
    tags = db.execute('SELECT tag FROM tags WHERE bug=?', (bug,))
    for tag in tags:
        if tagtext:
            tagtext += ' ' + tag[0]
        else:
            tagtext += 'Tags: ' + tag[0]

    if tagtext:
        print tagtext

    print

    if str(bug) != args.id:
        bugmails = mailbox.Maildir('btsmail/' + str(bug))
        msg = bugmails.get_file(key)
        if not msg:
            print 'Email message not available.'
        else:
            print msg.read()
    else:
        matches = db.execute('SELECT msgid, key FROM messages WHERE bug=?', (bug,))
        for i in matches:
            print i[0]


def do_search(args):
    bugs = db.execute('SELECT id, status, severity, title FROM bugs WHERE title LIKE ?', ('%' + ' '.join(args.term) + '%',))
    for i in bugs:
        print "{:>6} {:6} {:9}  {:}".format(i[0], statusnames[i[1]], severitynames[i[2]], i[3])

def do_create(args):
    global db, maildir
    print "Write the bug report here, press control-D on an empty line to stop:"
    msg = email.MIMEText.MIMEText(sys.stdin.read())
    title = ' '.join(args.title)
    id = email.utils.make_msgid('LightBTS')

    msg['Subject'] = title
    msg['Message-Id'] = id
    #TODO: get full name
    if os.environ['EMAIL']:
        msg['From'] = os.environ['EMAIL']
    else:
        msg['From'] = getpass.getuser() + '@' + platform.node()
    msg['To'] = 'LightBTS'
    msg['Date'] = email.utils.formatdate()
    key = maildir.add(msg)
    try:
        db.execute("INSERT INTO messages (key, msgid, bug) values (?,?,?)", (key, id, 0));
    except sqlite3.IntegrityError:
        print "Duplicate message!"
        sys.exit(1)
    bug = db.execute("INSERT INTO bugs (title) VALUES (?)", (title,)).lastrowid
    db.execute("UPDATE messages SET bug=? WHERE key=?", (bug, key))
    db.execute("INSERT OR IGNORE INTO recipients (bug, address) VALUES (?, ?)", (bug, msg['From']))
    maildir.close()
    maildir = mailbox.Maildir('btsmail/' + str(bug))
    movefrom = 'btsmail/new/' + key
    moveto = 'btsmail/' + str(bug) + '/cur/' + key
    os.rename(movefrom, moveto)
    print 'Thank you for reporting a bug, which has been assigned number ' + str(bug)

def do_reply(args):
    global db, maildir
    try:
        bug = int(args.id)
    except:
        bug = 0
    if str(bug) != args.id:
        bug = 0
        matches = db.execute('SELECT bug, key FROM messages WHERE msgid=?', (args.id,))
        for i in matches:
            if i[0]:
                bug = i[0]
                key = i[1]
        if not bug:
            print 'Could not find bug with Message-Id ' + args.id
            return
    matches = db.execute('SELECT title FROM bugs WHERE id=?', (bug,))
    for i in matches:
        title = i[0]
        break
    else:
        print 'Could not find bug #' + str(bug)
        return

    print 'Replying to bug ' + str(bug) + ' with title ' + title
    print "Write the reply here, press control-D on an empty line to stop:"
    msg = email.MIMEText.MIMEText(sys.stdin.read())
    id = email.utils.make_msgid('LightBTS')

    msg['Subject'] = 'Re: ' + title
    msg['Message-Id'] = id
    if str(bug) != args.id:
        msg['In-Reply-To'] = args.id
    #TODO: get full name
    if os.environ['EMAIL']:
        msg['From'] = os.environ['EMAIL']
    else:
        msg['From'] = getpass.getuser() + '@' + platform.node()
    msg['To'] = 'LightBTS'
    msg['Date'] = email.utils.formatdate()
    key = maildir.add(msg)
    try:
        db.execute("INSERT INTO messages (key, msgid, bug) values (?,?,?)", (key, id, bug));
    except sqlite3.IntegrityError:
        print "Duplicate message!"
        sys.exit(1)
    db.execute("INSERT OR IGNORE INTO recipients (bug, address) VALUES (?, ?)", (bug, msg['From']))
    maildir.close()
    maildir = mailbox.Maildir('btsmail/' + str(bug))
    movefrom = 'btsmail/new/' + key
    moveto = 'btsmail/' + str(bug) + '/cur/' + key
    os.rename(movefrom, moveto)
    print 'Thank you for reporting additional information for bug number ' + str(bug)

def do_close(args):
    db.execute('UPDATE bugs SET status=0 WHERE id=?', (args.id,))

def do_reopen(args):
    db.execute('UPDATE bugs SET status=1 WHERE id=?', (args.id,))

def do_retitle(args):
    title = ' '.join(args.title)
    db.execute('UPDATE bugs SET title=? WHERE id=?', (title, args.id))

def do_found(args):
    pass

def do_notfound(args):
    pass

def do_fixed(args):
    pass

def do_notfixed(args):
    pass

def do_severity(args):
    severity = severitynames.index(args.severity)
    db.execute('UPDATE bugs SET severity=? WHERE id=?', (severity, args.id))

def do_merge(args):
    a = min(args.id)
    for b in args.id:
        if a == b:
            continue
        db.execute('INSERT OR IGNORE INTO merges (a, b) VALUES (?, ?)', (a, b))

def do_unmerge(args):
    a = min(args.id)
    for b in args.id:
        if a == b:
            continue
        db.execute('DELETE FROM merges WHERE a=? AND b=?', (a, b))

def do_tags(args):
    add = True
    for tag in args.tags:
        if tag[0] == '-':
            add = False
            tag = tag[1:]
        elif tag[0] == '+':
            add = True
            tag = tag[1:]
        elif tag[0] == '=':
            add = True
            db.execute('DELETE FROM tags WHERE bug=?', (args.id,))
            tag = tag[1:]
        if tag:
            if add:
                db.execute('INSERT OR IGNORE INTO tags (bug, tag) VALUES (?, ?)', (args.id, tag))
            else:
                db.execute('DELETE FROM tags WHERE bug=? AND tag=?', (args.id, tag))

def do_owner(args):
    db.execute('UPDATE bugs SET owner=? WHERE id=?', (args.owner, args.id))

def do_noowner(args):
    db.execute('UPDATE bugs SET owner=NULL WHERE id=?', (args.id))

def do_spam(args):
    db.execute('UPDATE messages SET spam=1 WHERE msgid=?', (args.msgid,))

def do_nospam(args):
    db.execute('UPDATE messages SET spam=0 WHERE msgid=?', (args.msgid,))

# Initialize

parser = argparse.ArgumentParser(description='Manage bugs.', epilog='Report bugs to guus@sliepen.org.')
parser.add_argument('-d', '--data', metavar='DIR', help='directory where LightBTS stores its data')
parser.add_argument('--version', action='version', version='LightBTS ' + version)

subparser = parser.add_subparsers(title='commands', dest='command')

parser_init = subparser.add_parser('init', help='initialize bug database')
parser_init.set_defaults(func=do_init)

parser_list = subparser.add_parser('list', help='list bugs')
parser_list.add_argument('prop', help='bug property (status, severity, tag)', nargs='*')
parser_list.set_defaults(func=do_list)

parser_show = subparser.add_parser('show', help='show bug or message details')
parser_show.add_argument('id', help='bug or message id')
parser_show.set_defaults(func=do_show)

parser_search = subparser.add_parser('search', help='search bugs')
parser_search.add_argument('term', help='search term', nargs=argparse.REMAINDER)
parser_search.set_defaults(func=do_search)

parser_create = subparser.add_parser('create', help='create a new bug')
parser_create.add_argument('title', help='bug title', nargs=argparse.REMAINDER)
parser_create.set_defaults(func=do_create)

parser_reply = subparser.add_parser('reply', help='reply to an existing bug')
parser_reply.add_argument('id', help='bug or message id')
parser_reply.set_defaults(func=do_reply)

parser_close = subparser.add_parser('close', help='close an existing bug')
parser_close.add_argument('id', help='bug id')
parser_close.set_defaults(func=do_close)

parser_reopen = subparser.add_parser('reopen', help='reopen an existing bug')
parser_reopen.add_argument('id', help='bug id')
parser_reopen.set_defaults(func=do_reopen)

parser_retitle = subparser.add_parser('retitle', help='change title of an existing bug')
parser_retitle.add_argument('id', help='bug id')
parser_retitle.add_argument('title', help='new title', nargs=argparse.REMAINDER)
parser_retitle.set_defaults(func=do_retitle)

parser_found = subparser.add_parser('found', help='record version where the bug appears')
parser_found.add_argument('id', help='bug id')
parser_found.add_argument('version', help='project version')
parser_found.set_defaults(func=do_found)

parser_notfound = subparser.add_parser('notfound', help='remove record of version where the bug appears')
parser_notfound.add_argument('id', help='bug id')
parser_notfound.add_argument('version', help='project version')
parser_notfound.set_defaults(func=do_notfound)

parser_fixed = subparser.add_parser('fixed', help='record version where the bug is fixed')
parser_fixed.add_argument('id', help='bug id')
parser_fixed.add_argument('version', help='project version')
parser_fixed.set_defaults(func=do_fixed)

parser_notfixed = subparser.add_parser('notfixed', help='remove record of version where the bug is fixed')
parser_notfixed.add_argument('id', help='bug id')
parser_notfixed.add_argument('version', help='project version')
parser_notfixed.set_defaults(func=do_notfixed)

parser_severity = subparser.add_parser('severity', help='change bug severity')
parser_severity.add_argument('id', help='bug id')
parser_severity.add_argument('severity', help='bug id', choices=severitynames)
parser_severity.set_defaults(func=do_severity)

parser_merge = subparser.add_parser('merge', help='merge two or more bugs')
parser_merge.add_argument('id', help='bug ids', nargs='+')
parser_merge.set_defaults(func=do_merge)

parser_unmerge = subparser.add_parser('unmerge', help='unmerge previously merged bugs')
parser_unmerge.add_argument('id', help='bug ids', nargs='+')
parser_unmerge.set_defaults(func=do_unmerge)

parser_tags = subparser.add_parser('tags', help='add or remove tags')
parser_tags.add_argument('id', help='bug id')
parser_tags.add_argument('tags', help='tags, optionally prefixed with + or -', nargs=argparse.REMAINDER)
parser_tags.set_defaults(func=do_tags)

parser_owner = subparser.add_parser('owner', help='set the owner of a bug')
parser_owner.add_argument('id', help='bug id')
parser_owner.add_argument('email', help='owner email address')
parser_owner.set_defaults(func=do_owner)

parser_noowner = subparser.add_parser('noowner', help='remove record of the owner of a bug')
parser_noowner.add_argument('id', help='bug id')
parser_noowner.set_defaults(func=do_noowner)

parser_spam = subparser.add_parser('spam', help='mark a message as spam')
parser_spam.add_argument('id', help='bug id')
parser_spam.set_defaults(func=do_spam)

parser_nospam = subparser.add_parser('nospam', help='mark a message as not being spam')
parser_nospam.add_argument('id', help='bug id')
parser_nospam.set_defaults(func=do_nospam)

args = parser.parse_args()

if args.data:
    os.chdir(args.data)
else:
    os.chdir(os.environ['HOME'])

maildir = mailbox.Maildir('btsmail')
db = sqlite3.connect('bts.db')
db.execute('PRAGMA foreign_key = on')

db.execute('CREATE TABLE IF NOT EXISTS bugs (id INTEGER PRIMARY KEY AUTOINCREMENT, status INTEGER NOT NULL DEFAULT 1, severity INTEGER NOT NULL DEFAULT 2, title TEXT, owner TEXT, submitter TEXT)')
db.execute('CREATE TABLE IF NOT EXISTS merges (a INTEGER, b INTEGER, PRIMARY KEY(a, b), FOREIGN KEY(a) REFERENCES bugs(id), FOREIGN KEY(b) REFERENCES bugs(id))')
db.execute('CREATE TABLE IF NOT EXISTS messages (msgid PRIMARY KEY, key TEXT, bug INTEGER, spam INTEGER NOT NULL DEFAULT 0, FOREIGN KEY(bug) REFERENCES bugs(id))')
db.execute('CREATE INDEX IF NOT EXISTS msgid_index ON messages (msgid)')
db.execute('CREATE TABLE IF NOT EXISTS recipients (bug INTEGER, address TEXT, PRIMARY KEY(bug, address), FOREIGN KEY(bug) REFERENCES bugs(id))')
db.execute('CREATE TABLE IF NOT EXISTS tags (bug INTEGER, tag TEXT, PRIMARY KEY(bug, tag), FOREIGN KEY(bug) REFERENCES bugs(id))')

args.func(args)

db.commit()
db.close()
