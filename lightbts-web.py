#!/usr/bin/python
# coding: utf-8

import lightbts
import argparse
import os
import syslog
import string
import urlparse

copyright = 'LightBTS ' + lightbts.__version__ + ', copyright (c) 2014-2015 Guus Sliepen'

def init():
        lightbts.init(home)

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
        lightbts.exit()
        pass

def do_bug(environ, start_response, query):
    init()
    id = query['bug']
    bug = lightbts.get_bug(id);

    if not bug:
        start_response('404 NOT FOUND', [('Content-Type', 'text/plain; charset=utf-8')])
        return 'Bug ' + query['bug'] + ' does not exist.\n'

    start_response('200 OK', [('Content-Type', 'text/html; charset=utf-8')])

    msglist = ''
    for msg in bug.get_messages():
        msglist += tmpl_message.substitute({'msgid': msg.msg['Message-Id'], 'from': msg.msg['From'], 'to': msg.msg['To'], 'subject': msg.msg['Subject'], 'date': msg.msg['Date'], 'body': msg.msg.get_payload()})

    page = tmpl_bug.substitute({'id': bug.id, 'submitter': 'x', 'date': 'x', 'status': bug.statusname, 'severity': bug.severityname 'title': bug.title, 'msglist': msglist, 'root': lightbts.webroot, 'copyright': copyright})
    exit()
    return str(page)

def myapp(environ, start_response):
    query = dict(urlparse.parse_qsl(environ['QUERY_STRING']))
    if 'bug' in query:
        return do_bug(environ, start_response, query)

    start_response('200 OK', [('Content-Type', 'text/html; charset=utf-8')])

    init()
    buglist = ''
    class args(object):
        prop=()
    for bug in lightbts.list_bugs(args):
        buglist += tmpl_bugrow.substitute({'id': bug._id, 'status': bug.statusname, 'severity': bug.severityname, 'title': bug.title})
    page = tmpl_main.substitute({'buglist': str(buglist), 'copyright': copyright, 'root': lightbts.staticroot})
    exit()

    return page

if __name__ == '__main__':
        global home
	# Initialize
	syslog.syslog("LightBTS web starting")

	parser = argparse.ArgumentParser(description='LightBTS web frontend.', epilog='Report bugs to guus@sliepen.org.')
	parser.add_argument('-d', '--data', metavar='DIR', help='directory where LightBTS stores its data')
	parser.add_argument('--version', action='version', version='LightBTS ' + lightbts.__version__)

	args = parser.parse_args()

	if args.data:
	    home = args.data
	else:
	    home = os.environ['HOME']

	from flup.server.fcgi import WSGIServer
	WSGIServer(myapp).run()
