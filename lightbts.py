#!/usr/bin/python
# coding: utf-8

import os
import sys
import getpass
import platform
import email
import mailbox
import sqlite3
import string
import logging
import smtplib
try:
    import configparser
except ImportError:
    import ConfigParser as configparser

try:
    import pwd
    have_pwd = True
except ImportError:
    have_pwd = False

try:
    import win32api
    have_win32api = True
except ImportError:
    have_win32api = False

__version__ = '0.1'

db = None
mail = None
maildir = None
dbfile = None

statii = ['closed', 'open']
severities = ['wishlist', 'minor', 'normal', 'important', 'serious', 'critical', 'grave']

def severityname(nr):
    return severities[nr]

def severityindex(name):
    return severities.index(name)

def severity(arg):
    try:
        return severityname(arg)
    except:
        return severityindex(arg)

def statusname(nr):
    return statii[nr]

def statusindex(name):
    return statii.index(name)

def status(arg):
    try:
        return statusname(arg)
    except:
        return statusindex(arg)

def get_local_email_address():
    if 'EMAIL' in os.environ:
        address = os.environ['EMAIL']
        if address.find(" <") >= 0:
            return address
    else:
        address = getpass.getuser() + '@' + platform.node()

    if 'FULLNAME' in os.environ:
        fullname = os.environ['FULLNAME']
    else:
        if have_pwd:
            fullname = pwd.getpwuid(os.getuid())[4].split(",")[0]
        elif have_win32api:
            fullname = win32api.GetUserName(3)

    if fullname:
        return fullname + ' <' + address + '>'
    else:
        return address

class bug(object):
    def __init__(self, id=None, title=None, status=None, severity=None):
        self._id = id
        self._title = title
        self._status = status
        self._severity = severity

    def get_id(self):
        return self._id

    id = property(get_id)

    def get_status(self):
        return self._status

    def get_statusname(self):
        return statusname(self._status)

    def set_status(self, status):
        db.execute('UPDATE bugs SET status=? WHERE id=?', (status, self._id))
        self._status = status

    def set_statusname(self, name):
        set_status(statusindex(name))

    status = property(get_status, set_status)
    statusname = property(get_statusname, set_statusname)

    def set_version_status(self, version, status):
        db.execute('INSERT OR REPLACE INTO versions (bug, version, status) VALUES (?, ?, ?)', (self._id, version, status))

    def get_title(self):
        return self._title

    def set_title(self, title):
        db.execute('UPDATE bugs SET title=? WHERE id=?', (title, self._id))
        self._title = title

    title = property(get_title, set_title)

    def get_severity(self):
        return self._severity

    def get_severityname(self):
        return severityname(self._severity)

    def set_severity(self, severity):
        code = severities.index(severity)
        db.execute('UPDATE bugs SET severity=? WHERE id=?', (code, self._id))
        self._severity = severity

    def set_severityname(self, name):
        set_severity(severityinex(name))

    severity = property(get_severity, set_severity)
    severityname = property(get_severityname, set_severityname)

    def add_tag(self, tag):
        db.execute('INSERT OR IGNORE INTO tags (bug, tag) VALUES (?, ?)', (self._id, tag))

    def del_tag(self, tag):
        db.execute('DELETE FROM tags WHERE bug=? AND tag=?', (self._id, tag))

    def clear_tags(self):
        db.execute('DELETE FROM tags WHERE bug=?', (self._id,))

    def get_tags(self):
        result = []
        for i in db.execute('SELECT tag FROM tags WHERE bug=?', (self._id,)):
            result.append(i[0])
        return result

    def clear_tags(self):
        db.execute('DELETE FROM tags WHERE bug=?', (self._id,))

    def modify_tags(self, *tags):
        add = True
        for tag in tags:
            if tag[0] == '-':
                add = False
                tag = tag[1:]
            elif tag[0] == '+':
                add = True
                tag = tag[1:]
            elif tag[0] == '=':
                add = True
                self.clear_tags()
                tag = tag[1:]
            if tag:
                if add:
                    self.add_tag(tag)
                else:
                    self.del_tag(tag)

    def set_owner(self, owner):
        if owner:
            db.execute('UPDATE bugs SET owner=? WHERE id=?', (owner, self._id))
        else:
            db.execute('UPDATE bugs SET owner=NULL WHERE id=?', (owner, self._id))

    def close(self):
        self.set_status(0)

    def reopen(self):
        self.set_status(1)

    def fixed(self, version):
        self.set_version_status(version, 0);

    def notfixed(self, version):
        db.execute('DELETE FROM versions WHERE bug=? AND version=? AND status=0', (self._id, version))

    def found(self, version):
        self.set_version_status(version, 1);

    def notfound(self, version):
        db.execute('DELETE FROM versions WHERE bug=? AND version=? AND status=1', (self._id, version))

    def get_found_versions(self):
        result = []
        for i in db.execute('SELECT version FROM versions WHERE bug=? AND status=1', (self._id,)):
            result.append(i[0])
        return result

    def get_fixed_versions(self):
        result = []
        for i in db.execute('SELECT version FROM versions WHERE bug=? AND status=0', (self._id,)):
            result.append(i[0])
        return result

    def get_messages(self):
        result = []
        for i in db.execute('SELECT msgid, key FROM messages WHERE bug=? AND key NOT NULL', (self._id,)):
            result.append(message(i[0], i[1], self._id))
        return result

    def get_first_msgid(self):
        return db.execute('SELECT msgid FROM messages WHERE bug=?', (self._id,)).fetchone()[0]

    def record_action(self, action, address=get_local_email_address()):
        msg = create_message(self.title, address, action, headers={
            'X-LightBTS-Control': 'yes',
            'In-Reply-To': self.get_first_msgid()
        });
        msg.assign_to(self)

class message(object):
    def __init__(self, msgid=None, key=None, bug=None, msg=None):
        self._msgid = msgid
        self._key = key
        self._bug = bug
        self._msg = msg

    def get_msgid(self):
        return self._msgid

    msgid = property(get_msgid)

    def get_key(self):
        return self._key

    key = property(get_key)

    def get_bug(self):
        return self._bug

    bug = property(get_bug)

    def get_msg(self):
        global maildir
        if not self._msg:
            bugmaildir = mailbox.Maildir(os.path.join(maildir, str(self._bug)))
            self._msg = email.Parser.Parser().parse(bugmaildir.get_file(self._key))
        return self._msg;

    msg = property(get_msg)

    def get_headers():
        return ''

    def get_body():
        return ''

    def set_spam(self, value=True):
        db.execute('UPDATE messages SET spam=? WHERE msgid=?', (value, self._msgid))

    def assign_to(self, bug):
        mailbox.Maildir(os.path.join(maildir, str(bug.id))).close()
        movefrom = os.path.join(maildir, 'new', self._key)
        moveto = os.path.join(maildir, str(bug.id), 'cur', self._key)
        os.rename(movefrom, moveto)
     
        db.execute('UPDATE messages SET bug=? WHERE key=?', (bug.id, self._key))
        db.execute('INSERT OR IGNORE INTO recipients (bug, address) VALUES (?, ?)', (bug.id, self._msg['From']))

def list_bugs(args):
    """List bugs, optionally filter on status, severity and/or tags."""
    cmd = 'SELECT id, title, status, severity FROM bugs WHERE 1'
    cmdargs = []
    status = 1
    severities = []
    tags = []
    for i in args.prop:
        if i == 'all':
            status = None
        elif i in statii:
            status = statusindex(i)
        elif i in severities:
            severities.append(severityindex(i))
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
            
    bugs = []
    for i in db.execute(cmd, tuple(cmdargs)):
        bugs.append(bug(int(i[0]), i[1], i[2], i[3]))

    return bugs

def search_bugs(args):
    bugs = []
    for i in db.execute('SELECT id, title, status, severity FROM bugs WHERE title LIKE ?', (('%' + ' '.join(args.term) + '%'),)):
        bugs.append(bug(int(i[0]), i[1], i[2], i[3]))
    return bugs

def get_bug(bugno):
    for i in db.execute('SELECT id, title, status, severity FROM bugs WHERE id=?', (bugno,)):
        return bug(int(i[0]), i[1], i[2], i[3])
    return None

def get_bug_from_title(title):
    for i in db.execute('SELECT id, title, status, severity FROM bugs WHERE title=?', (title,)):
        return bug(int(i[0]), i[1], i[2], i[3])
    return None

def get_message(msgid):
    for i in db.execute('SELECT msgid, key, bug FROM messages WHERE msgid=?', (email.utils.unquote(msgid),)):
        return message(i[0], i[1], int(i[2]))
    return None

def get_bug_from_msgid(msgid):
    return get_bug(get_message(msgid)._bug)

def merge(*bugs):
    a = min(bugs.bugno)
    for b in bugs:
        if a == b:
            continue
        db.execute('INSERT OR IGNORE INTO merges (a, b) VALUES (?, ?)', (a.bugno, b.bugno))

def unmerge(*bugs):
    a = min(bugs.bugno)
    for b in bugs:
        if a == b:
            continue
        db.execute('DELETE FROM merges WHERE a=? AND b=?', (a.bugno, b.bugno))

def get_template(name, fallback=None):
    try:
        with open(os.path.join(emailtemplates, name), 'r') as file:
            return string.Template(file.read())
    except IOError:
        try:
            with open(os.path.join(default_templates, name), 'r') as file:
                return string.Template(file.read())
        except:
            logging.error("Template " + name + " not found")
            if fallback:
                return string.Template(fallback)
            else:
                return None

def init_db(dbfile):
    db = sqlite3.connect(dbfile)
    db.execute('PRAGMA foreign_key = on')
    version = db.execute('PRAGMA user_version').fetchone()[0]
    appid = db.execute('PRAGMA application_id').fetchone()
    if appid:
        appid = appid[0]

    if appid or (appid == 0 and version):
        if appid != 0x4c425453:
            logging.error("Index is a SQLite database not created by LightBTS!")
            sys.exit(1)

    if not appid:
        db.execute('PRAGMA application_id=0x4c425453')

    if not version:
        # Fresh start, create everything.
        db.execute('CREATE TABLE bugs (id INTEGER PRIMARY KEY AUTOINCREMENT, status INTEGER NOT NULL DEFAULT 1, severity INTEGER NOT NULL DEFAULT 2, title TEXT, owner TEXT, submitter TEXT, date INTEGER)')
        db.execute('CREATE TABLE merges (a INTEGER, b INTEGER, PRIMARY KEY(a, b), FOREIGN KEY(a) REFERENCES bugs(id), CHECK (a < b), FOREIGN KEY(b) REFERENCES bugs(id))')
        db.execute('CREATE TABLE messages (msgid PRIMARY KEY, key TEXT, bug INTEGER, spam INTEGER NOT NULL DEFAULT 0, date INTEGER, FOREIGN KEY(bug) REFERENCES bugs(id))')
        db.execute('CREATE INDEX messages_key_index ON messages (key)')
        db.execute('CREATE TABLE recipients (bug INTEGER, address TEXT, PRIMARY KEY(bug, address), FOREIGN KEY(bug) REFERENCES bugs(id))')
        db.execute('CREATE INDEX recipients_bug_index ON recipients (bug)')
        db.execute('CREATE INDEX recipients_address_index ON recipients (address)')
        db.execute('CREATE TABLE tags (bug INTEGER, tag TEXT, PRIMARY KEY(bug, tag), FOREIGN KEY(bug) REFERENCES bugs(id))')
        db.execute('CREATE INDEX tags_bug_index ON tags (bug)')
        db.execute('CREATE INDEX tags_tag_index ON tags (tag)')
        db.execute('CREATE TABLE versions (bug INTEGER, version TEXT, status INTEGER NOT NULL DEFAULT 1, PRIMARY KEY(bug, version))')
        db.execute('CREATE INDEX versions_bug_index ON versions (bug)')
        db.execute('CREATE INDEX versions_version_index ON versions (version)')
        db.execute('PRAGMA user_version=1')
        return db
    elif version == 1:
        # Latest version, nothing to do.
        return db
    elif version < 0 or version > 1:
        logging.error("Unknown database version " + str(version))
        sys.exit(1)

def init(dir=''):
    global basedir, config, dbfile, maildir, project, admin, respond_to_new, respond_to_reply
    global emailaddress, emailname, emailtemplates, smtphost, webroot, staticroot, webtemplates
    global db, mail, default_templates

    basedir = dir
    default_templates = os.path.join(os.path.split(__file__)[0], "templates")

    # Set default options

    config = configparser.SafeConfigParser()

    config.add_section('core')
    config.set('core', 'project', '')
    config.set('core', 'admin', '')
    config.set('core', 'messages', os.path.join(basedir, 'messages'))
    config.set('core', 'index', os.path.join(basedir, 'index'))
    config.set('core', 'respond-to-new', 'yes')
    config.set('core', 'respond-to-reply', 'yes')

    config.add_section('web')
    config.set('web', 'root', '/')
    config.set('web', 'static-root', '/')
    config.set('web', 'templates', os.path.join(basedir, 'templates'))

    config.add_section('email')
    config.set('email', 'address', getpass.getuser() + '@' + platform.node())
    config.set('email', 'name', '')
    config.set('email', 'templates', os.path.join(basedir, 'templates'))
    config.set('email', 'smtphost', 'localhost')

    # Read the configuration file

    try:
        os.makedirs(basedir)
    except:
        pass

    config.read(os.path.join(basedir, 'lightbts.conf'))

    # Core configuration
    dbfile = config.get('core', 'index')
    maildir = config.get('core', 'messages')
    project = config.get('core', 'project')
    admin = config.get('core', 'admin')
    respond_to_new = config.getboolean('core', 'respond-to-new')
    respond_to_reply = config.getboolean('core', 'respond-to-reply')

    # Email configuration
    emailaddress = config.get('email', 'address')
    emailname = config.get('email', 'name')
    if not emailname:
        emailname = project
    emailtemplates = config.get('email', 'templates')
    smtphost = config.get('email', 'smtphost')

    # Web configuration
    webroot = config.get('web', 'root')
    staticroot = config.get('web', 'static-root')
    if not staticroot:
        staticroot = webroot
    webtemplates = config.get('web', 'templates')

    # Open the mailbox
    mail = mailbox.Maildir(maildir)

    # Open or create the database
    db = init_db(dbfile)

def exit():
    global basedir, db, mail, config

    db.commit()
    db.close()
    db = None

    mail.close()
    mail = None

    config.remove_section('DEFAULT')
    config.write(open(os.path.join(basedir, 'lightbts.conf'), 'w'))

def create_message(title, address, text, headers={}):
    global db, mail

    msg = email.MIMEText.MIMEText(text)

    msg['Subject'] = title
    msg['Message-ID'] = email.utils.make_msgid('LightBTS')
    msg['From'] = address
    msg['To'] = 'LightBTS'
    msg['Date'] = email.utils.formatdate()
    msg['User-Agent'] = 'LightBTS/' + __version__

    for key, value in headers.iteritems():
        msg[key] = value

    msgid = email.utils.unquote(msg['Message-ID'])

    # Save message to new

    key = mail.add(msg)

    # Store the message in the database

    try:
        db.execute("INSERT INTO messages (key, msgid, bug) values (?,?,?)", (key, msgid, 0));
    except sqlite3.IntegrityError:
        # TODO: throw something intelligent
        logging.error("Integrity error while creating a new message")
        return None

    return message(msgid, key, msg=msg)

def create_bug(title):
    global db

    bugno = db.execute("INSERT INTO bugs (title) VALUES (?)", (title,)).lastrowid

    return bug(bugno, title, 1, 2)

def create(title, address, text):
    msg = create_message(title, address, text)
    bug = create_bug(title)
    msg.assign_to(bug)
    return bug
 
def reply(bug, address, text):
    msg = create_message(bug.title, address, text)
    msg.assign_to(bug)

def forward_message(bug, msg):
    # Get email addresses of bug participants
    # TODO: handle -quiet, -maintonly, -submitter here.

    do = [admin]
    for i in db.execute("SELECT address FROM recipients WHERE bug=?", (bug._id,)):
        do.append(i[0])

    do = set(map((lambda x: x[1]), email.utils.getaddresses(do)))

    # Get all To: and Cc: addresses from the message

    dont = msg.get_all('From', []) + msg.get_all('To', []) + msg.get_all('Cc', [])
    dont = set(map((lambda x: x[1]), email.utils.getaddresses(dont)))

    # Send a copy to those who didn't get the message yet

    do -= dont

    if do:
        smtp = smtplib.SMTP(smtphost)
        smtp.sendmail(msg['From'], do, msg.as_string())

def import_email(msg):
    # Don't allow messages with the X-LightBTS-Control header set

    if msg['X-LightBTS-Control']:
        logging.error("Denying import of message with X-LightBTS-Control header from " + msg['From'])
        return (None, None)

    # Handle missing Message-ID

    if not msg['Message-ID']:
        msg['Message-ID'] = email.utils.make_msgid('LightBTS')

    # Save message to new

    msgid = email.utils.unquote(msg['Message-ID'])
    parent = email.utils.unquote(msg['In-Reply-To'])
    subject = msg['Subject']
    key = mail.add(msg)

    # Store the message in the database

    try:
        db.execute("INSERT INTO messages (key, msgid, bug) values (?,?,?)", (key, msgid, 0));
    except sqlite3.IntegrityError:
        # TODO: throw something intelligent
        logging.warning("Ignoring duplicate message from " + msg['From'] + " with Message-ID " + msgid)
        return (None, None)

    # Can we match the message to an existing bug?

    bugno = 0
    new = False

    matches = db.execute("SELECT bug FROM messages WHERE msgid=?", (parent,))
    for i in matches:
        if i[0]:
            bugno = i[0]

    # Try finding one with a similar subject
    # TODO: more robust?

    if not bugno:
            db.execute("SELECT id FROM bugs WHERE title LIKE ?", ('%' + subject,))
            for i in matches:
                if i[0]:
                    bugno = i[0]

    if not bugno:
        bugno = db.execute("INSERT INTO bugs (title) VALUES (?)", (subject,)).lastrowid
        new = True

    bug = get_bug(bugno)

    db.execute("UPDATE messages SET bug=? WHERE key=?", (bugno, key))

    db.execute("INSERT OR IGNORE INTO recipients (bug, address) VALUES (?, ?)", (bugno, msg['From']))

    # Move the message to the appropriate folder

    bugdir = os.path.join(maildir, str(bugno))
    mailbox.Maildir(bugdir).close()
    movefrom = os.path.join(maildir, 'new', key)
    moveto = os.path.join(bugdir, 'cur', key)
    os.rename(movefrom, moveto)

    # Email a copy to interested people

    forward_message(bug, msg)

    return (bug, new)

def record_msgid(bugno, msgid):
    db.execute("INSERT INTO messages (msgid, bug) values (?,?)", (msgid, bugno));
