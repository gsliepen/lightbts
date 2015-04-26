#!/usr/bin/python

import os
import argparse
import sys

import lightbts

severitynames = ['wishlist', 'minor', 'normal', 'important', 'serious', 'grave', 'critical']

def do_init(args):
    print 'LightBTS initialized.'

def do_list(args):
    for bug in lightbts.list_bugs(args):
        print "{:>6} {:6} {:9}  {:}".format(bug.id, lightbts.statusname(bug.status), lightbts.severityname(bug.severity), bug.title)

def do_show_bug(bugno, verbose):
    bug = lightbts.get_bug(bugno)
    if not bug:
        print 'Could not find bug #' + str(bugno)
        return

    found_versions = bug.get_found_versions()
    fixed_versions = bug.get_fixed_versions()
    print 'Bug#' + str(bug.id) + ': ' + bug.title
    print 'Status: ' + lightbts.statusname(bug.status)
    if found_versions:
        print 'Found in: ' + ' '.join(found_versions)
    if fixed_versions:
        print 'Fixed in: ' + ' '.join(fixed_versions)
    print 'Severity: ' + lightbts.severityname(bug.severity)
    tags = bug.get_tags()
    if tags:
        print 'Tags: ' + ' '.join(tags)

    print

    for msg in bug.get_messages():
        if(verbose):
            print 'From: ' + msg.msg['From']
            print 'To: ' + msg.msg['To']
            print 'Subject: ' + msg.msg['Subject']
            print 'Date: ' + msg.msg['Date']
            print 'Message-Id: ' + msg.msg['Message-Id']
            print
            print msg.msg.get_payload();
            print
        else:
            print msg.msgid

def do_show_message(msgid):
    msg = lightbts.get_message(msgid)
    if not msg:
        print 'Could not find message with id ' + msgid
        return

    bug = lightbts.get_bug(msg.bug)
    found_versions = bug.get_found_versions()
    fixed_versions = bug.get_fixed_versions()
    print 'Bug#' + str(bug.id) + ': ' + bug.title
    print 'Status: ' + lightbts.statusname(bug.status)
    if found_versions:
        print 'Found in: ' + ' '.join(found_versions)
    if fixed_versions:
        print 'Fixed in: ' + ' '.join(fixed_versions)
    print 'Severity: ' + lightbts.severityname(bug.severity)
    tags = bug.get_tags()
    if tags:
        print 'Tags: ' + tags
    print
    print 'From: ' + msg.msg['From']
    print 'To: ' + msg.msg['To']
    print 'Subject: ' + msg.msg['Subject']
    print 'Date: ' + msg.msg['Date']
    print
    print msg.msg.get_payload();

def do_show(args):
    if '@' in args.id:
        return do_show_message(args.id)
    else:
        return do_show_bug(int(args.id), args.verbose)

def do_search(args):
    for bug in lightbts.search_bugs(args):
        print "{:>6} {:6} {:9}  {:}".format(bug.id, lightbts.statusname(bug.status), lightbts.severityname(bug.severity), bug.title)

def do_create(args):
    title = ' '.join(args.title)
    address = lightbts.get_local_email_address()
    print "Write the bug report here, press control-D on an empty line to stop:"
    text = sys.stdin.read();
    bug = lightbts.create(title, address, text);
    if args.version:
        bug.found(args.version)
    if args.tag:
        bug.add_tag(args.tag)
    print 'Thank you for reporting a bug, which has been assigned number ' + str(bug.id)

def do_reply(args):
    if args.close and args.reopen:
        print 'Make up your mind!'
        return

    bug = lightbts.get_bug(args.id)
    if not bug:
        print 'Could not find bug #' + str(bugno)
        return

    address = lightbts.get_local_email_address()
    print "Write the bug reply here, press control-D on an empty line to stop:"
    text = sys.stdin.read();
    lightbts.reply(bug, address, text);
    if args.version:
        if args.close:
            bug.fixed(args.version)
        else:
            bug.found(args.version)
    if args.tag:
        bug.add_tag(args.tag)
    if args.close:
        bug.close()
    if args.reopen:
        bug.reopen()
    print 'Thank you for reporting additional information for bug number ' + str(bug.id)

def do_close(args):
    lightbts.get_bug(args.id).close()
    if args.version:
        bug.fixed(args.version)
    if args.tag:
        bug.add_tag(args.tag)

def do_reopen(args):
    lightbts.get_bug(args.id).reopen()
    if args.version:
        bug.found(args.version)
    if args.tag:
        bug.add_tag(args.tag)

def do_retitle(args):
    lightbts.get_bug(args.id).set_title(' '.join(args.title))

def do_found(args):
    lightbts.get_bug(args.id).found(args.version)

def do_notfound(args):
    lightbts.get_bug(args.id).notfound(args.version)

def do_fixed(args):
    lightbts.get_bug(args.id).fixed(args.version)

def do_notfixed(args):
    lightbts.get_bug(args.id).notfixed(args.version)

def do_severity(args):
    severity = severitynames.index(args.severity)
    lightbts.db.execute('UPDATE bugs SET severity=? WHERE id=?', (severity, args.id))

def do_merge(args):
    a = min(args.id)
    for b in args.id:
        if a == b:
            continue
        lightbts.db.execute('INSERT OR IGNORE INTO merges (a, b) VALUES (?, ?)', (a, b))

def do_unmerge(args):
    a = min(args.id)
    for b in args.id:
        if a == b:
            continue
        lightbts.db.execute('DELETE FROM merges WHERE a=? AND b=?', (a, b))

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
            lightbts.db.execute('DELETE FROM tags WHERE bug=?', (args.id,))
            tag = tag[1:]
        if tag:
            if add:
                lightbts.db.execute('INSERT OR IGNORE INTO tags (bug, tag) VALUES (?, ?)', (args.id, tag))
            else:
                lightbts.db.execute('DELETE FROM tags WHERE bug=? AND tag=?', (args.id, tag))

def do_owner(args):
    lightbts.db.execute('UPDATE bugs SET owner=? WHERE id=?', (args.owner, args.id))

def do_noowner(args):
    lightbts.db.execute('UPDATE bugs SET owner=NULL WHERE id=?', (args.id))

def do_spam(args):
    lightbts.db.execute('UPDATE messages SET spam=1 WHERE msgid=?', (args.msgid,))

def do_nospam(args):
    lightbts.db.execute('UPDATE messages SET spam=0 WHERE msgid=?', (args.msgid,))

# Initialize

parser = argparse.ArgumentParser(description='Manage bugs.', epilog='Report bugs to guus@sliepen.org.')
parser.add_argument('-d', '--data', metavar='DIR', help='directory where LightBTS stores its data')
parser.add_argument('--version', action='version', version='LightBTS ' + lightbts.version)

subparser = parser.add_subparsers(title='commands', dest='command')

parser_init = subparser.add_parser('init', help='initialize bug database')
parser_init.set_defaults(func=do_init)

parser_list = subparser.add_parser('list', help='list bugs')
parser_list.add_argument('prop', help='bug property (status, severity, tag)', nargs='*')
parser_list.set_defaults(func=do_list)

parser_show = subparser.add_parser('show', help='show bug or message details')
parser_show.add_argument('-v', '--verbose', help='show full text of all messages associated to the bug', action='store_true')
parser_show.add_argument('id', help='bug or message id')
parser_show.set_defaults(func=do_show)

parser_search = subparser.add_parser('search', help='search bugs')
parser_search.add_argument('term', help='search term', nargs=argparse.REMAINDER)
parser_search.set_defaults(func=do_search)

parser_create = subparser.add_parser('create', help='create a new bug')
parser_create.add_argument('-v', '--version', metavar='VERSION', help='mark the bug as found in the given version')
parser_create.add_argument('-t', '--tag', metavar='TAG', help='set the given tag')
parser_create.add_argument('title', help='bug title', nargs=argparse.REMAINDER)
parser_create.set_defaults(func=do_create)

parser_reply = subparser.add_parser('reply', help='reply to an existing bug')
parser_reply.add_argument('-c', '--close', help='close the bug', action='store_true')
parser_reply.add_argument('-r', '--reopen', help='reopen the bug', action='store_true')
parser_reply.add_argument('-v', '--version', metavar='VERSION', help='mark the bug as found (fixed if the -c flag is used) in the given version')
parser_reply.add_argument('-t', '--tag', metavar='TAG', help='set the given tag')
parser_reply.add_argument('id', help='bug or message id')
parser_reply.set_defaults(func=do_reply)

parser_close = subparser.add_parser('close', help='close an existing bug')
parser_close.add_argument('-v', '--version', metavar='VERSION', help='mark the bug as fixed in the given version')
parser_close.add_argument('-t', '--tag', metavar='TAG', help='set the given tag')
parser_close.add_argument('id', help='bug id')
parser_close.set_defaults(func=do_close)

parser_reopen = subparser.add_parser('reopen', help='reopen an existing bug')
parser_reopen.add_argument('-v', '--version', metavar='VERSION', help='mark the bug as found in the given version')
parser_reopen.add_argument('-t', '--tag', metavar='TAG', help='set the given tag')
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
parser_severity.add_argument('severity', help='bug id', choices=lightbts.severities)
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
    lightbts.init(args.data)
else:
    lightbts.init(os.environ['HOME'])

args.func(args)

lightbts.exit()
