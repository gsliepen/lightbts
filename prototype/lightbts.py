#!/usr/bin/python3.6
# coding: utf-8

import os
import sys
import getpass
import platform
import email
import mailbox
import re
import sqlite3
import string
import logging
import smtplib
import dateutil
import subprocess
import glob
import ctypes
import binascii
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
hookdir = None
dbfile = None
quiet = False
no_hooks = False
emailaddress = None
admin = None
respond_to_new = False
respond_to_reply = False

statii = ['closed', 'open']
severities = ['wishlist', 'minor', 'normal', 'important', 'serious', 'critical', 'grave']
linktypes = ['relates', 'duplicates', 'depends', 'blocks']
verbose_linktypes = ['relates to', 'duplicates', 'depends on', 'blocks']
reverse_linktypes = ['related to by', 'duplicated by', 'depended on by', 'blocked by']

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

def linktypename(nr):
    return linktypes[nr]

def linktypeindex(name):
    return linktypes.index(name)

def linktype(arg):
    try:
        return linktypename(arg)
    except:
        return linktypeindex(arg)

def get_local_email_address():
    global admin

    if admin:
        return admin

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

# Python < 3.5 doesn't support the blake2b hash function, so borrow it from libb2.
libb2 = ctypes.cdll.LoadLibrary('libb2.so')

def blake2b(data, outlen=32):
    out = ctypes.create_string_buffer(outlen)
    result = libb2.blake2b(out, data, None, outlen, len(data), 0)
    assert result == 0
    return binascii.hexlify(out.raw).decode()

def store(msg):
    msg_id = email.utils.unquote(msg['Message-ID']).encode()
    hash = blake2b(msg_id, 24)
    tmpname = os.path.join(maildir, "tmp", hash)
    filename = os.path.join(maildir, hash[0:2], hash[2:])
    file = open(tmpname, "w")
    file.write(msg.as_string())
    file.close()
    try:
        os.mkdir(os.path.join(maildir, hash[0:2]))
    except OSError:
        pass

    os.rename(tmpname, filename)
    return filename

def load(msg_id):
    hash = blake2b(email.utils.unquote(msg_id).encode(), 24)
    filename = os.path.join(maildir, hash[0:2], hash[2:])
    msg = email.message_from_file(open(filename, "r"))

    return msg

class bug(object):
    def __init__(self, id=None, title=None, status=None, severity=None, owner=None, submitter=None, date=None, deadline=None, progress=0, milestone=None):
        self._id = id
        self._title = title
        self._status = status
        self._severity = severity
        self._owner = owner
        self._submitter = submitter
        self._date = date
        self._deadline = deadline
        self._progress = progress
        self._milestone = milestone

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
        db.execute('UPDATE bugs SET severity=? WHERE id=?', (severity, self._id))
        self._severity = severity

    def set_severityname(self, name):
        set_severity(severityindex(name))

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

    def modify_tags(self, tags):
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

    def get_owner(self):
        return self._owner

    def set_owner(self, owner):
        if owner:
            db.execute('UPDATE bugs SET owner=? WHERE id=?', (owner, self._id))
        else:
            db.execute('UPDATE bugs SET owner=NULL WHERE id=?', (self._id,))
        self._owner = owner

    owner = property(get_owner, set_owner)

    def get_submitter(self):
        return self._submitter

    submitter = property(get_submitter)

    def get_deadline(self):
        return self._deadline

    def set_deadline(self, deadline):
        if deadline:
            db.execute('UPDATE bugs SET deadline=? WHERE id=?', (deadline, self._id))
        else:
            db.execute('UPDATE bugs SET deadline=NULL WHERE id=?', (self._id,))
        self._deadline = deadline

    deadline = property(get_deadline, set_deadline)

    def get_milestone(self):
        return self._milestone

    def set_milestone(self, milestone):
        if milestone:
            db.execute('UPDATE bugs SET milestone=? WHERE id=?', (milestone, self._id))
        else:
            db.execute('UPDATE bugs SET milestone=NULL WHERE id=?', (self._id,))

    milestone = property(get_milestone, set_milestone)

    def get_progress(self):
        return self._progress

    def set_progress(self, progress):
        if progress or progress == 0:
            db.execute('UPDATE bugs SET progress=? WHERE id=?', (progress, self._id))
        else:
            db.execute('UPDATE bugs SET progress=NULL WHERE id=?', (self._id,))

    progress = property(get_progress, set_progress)

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

    def link(self, linktype, *bugs):
        for other in bugs:
            db.execute('INSERT OR IGNORE INTO links (a, b, type) VALUES (?, ?, ?)', (self._id, other._id, linktype))

    def unlink(self, linktype, *bugs):
        for other in bugs:
            db.execute('DELETE FROM links WHERE a=? AND b=? AND type=?', (self._id, other._id, linktype))

    def get_links(self):
        result = {}
        for i in db.execute('SELECT b, type FROM links WHERE a=?', (self._id,)):
            if i[1] not in result:
                result[i[1]] = set()
            result[i[1]].add(i[0])
        return result

    def get_reverse_links(self):
        result = {}
        for i in db.execute('SELECT a, type FROM links WHERE b=?', (self._id,)):
            if i[1] not in result:
                result[i[1]] = set()
            result[i[1]].add(i[0])
        return result

    def get_messages(self):
        result = []
        for i in db.execute('SELECT msgid FROM messages WHERE bug=?', (self._id,)):
            result.append(message(i[0], self._id))
        return result

    def get_first_msgid(self):
        return db.execute('SELECT msgid FROM messages WHERE bug=? LIMIT 1', (self._id,)).fetchone()[0]

    def record_action(self, action, address=get_local_email_address()):
        msg = create_message(self.title, address, action, headers={
            'X-LightBTS-Control': 'yes',
            'In-Reply-To': '<' + self.get_first_msgid() + '>'
        });
        msg.assign_to(self)

class message(object):
    def __init__(self, msgid=None, bug=None, msg=None):
        self._msgid = msgid
        self._bug = bug
        self._msg = msg

    def get_msgid(self):
        return self._msgid

    msgid = property(get_msgid)

    def get_bug(self):
        return self._bug

    bug = property(get_bug)

    def get_msg(self):
        global maildir
        if not self._msg:
            self._msg = load(self._msgid)
        return self._msg;

    msg = property(get_msg)

    def get_headers(self):
        return ''

    def get_body(self):
        msg = self.get_msg()
        text = None
        html = None
        for part in msg.walk():
            if part.get_content_type() == "text/plain":
                text = part
                break
            if part.get_content_type() == "text/html":
                html = part

        if text:
            return text.get_payload()

        if html:
            return html.payload()

        return ''

    def set_spam(self, value=True):
        db.execute('UPDATE messages SET spam=? WHERE msgid=?', (value, self._msgid))

    def assign_to(self, bug):
        db.execute('UPDATE messages SET bug=? WHERE msgid=?', (bug.id, self._msgid))
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
        bugs.append(bug(int(i[0]), i[1], i[2], int(i[3])))

    return bugs

def search_bugs(args):
    bugs = []
    for i in db.execute('SELECT id, title, status, severity FROM bugs WHERE title LIKE ?', (('%' + ' '.join(args.term) + '%'),)):
        bugs.append(bug(int(i[0]), i[1], i[2], int(i[3])))
    return bugs

def get_bug(bugno):
    for i in db.execute('SELECT id, title, status, severity, owner, submitter, date, deadline, progress, milestone FROM bugs WHERE id=?', (bugno,)):
        return bug(int(i[0]), title = i[1], status = int(i[2]), severity = int(i[3]), owner = i[4], submitter = i[5], date = i[6], deadline = i[7], progress = i[8], milestone = i[9])
    return None

def get_bug_from_title(title):
    for i in db.execute('SELECT id, title, status, severity FROM bugs WHERE title=?', (title,)):
        return bug(int(i[0]), i[1], i[2], i[3])
    return None

def get_message(msgid):
    for i in db.execute('SELECT msgid, bug FROM messages WHERE msgid=?', (email.utils.unquote(msgid),)):
        return message(i[0], int(i[1]))
    return None

def get_bug_from_msgid(msgid):
    return get_bug(get_message(msgid)._bug)

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
        db.execute('CREATE TABLE bugs (id INTEGER PRIMARY KEY AUTOINCREMENT, status INTEGER NOT NULL DEFAULT 1, severity INTEGER NOT NULL DEFAULT 2, title TEXT, owner TEXT, submitter TEXT, date INTEGER, deadline INTEGER, progress INTEGER NOT NULL DEFAULT 0, milestone TEXT)')
        db.execute('CREATE TABLE links (a INTEGER, b INTEGER, type INTEGER, PRIMARY KEY(a, b), FOREIGN KEY(a) REFERENCES bugs(id), FOREIGN KEY(b) REFERENCES bugs(id))')
        db.execute('CREATE INDEX links_a_index ON links (a)')
        db.execute('CREATE INDEX links_b_index ON links (b)')
        db.execute('CREATE TABLE messages (msgid PRIMARY KEY, bug INTEGER, spam INTEGER NOT NULL DEFAULT 0, date INTEGER, FOREIGN KEY(bug) REFERENCES bugs(id))')
        db.execute('CREATE TABLE recipients (bug INTEGER, address TEXT, PRIMARY KEY(bug, address), FOREIGN KEY(bug) REFERENCES bugs(id))')
        db.execute('CREATE INDEX recipients_bug_index ON recipients (bug)')
        db.execute('CREATE INDEX recipients_address_index ON recipients (address)')
        db.execute('CREATE TABLE tags (bug INTEGER, tag TEXT, PRIMARY KEY(bug, tag), FOREIGN KEY(bug) REFERENCES bugs(id))')
        db.execute('CREATE INDEX tags_bug_index ON tags (bug)')
        db.execute('CREATE INDEX tags_tag_index ON tags (tag)')
        db.execute('CREATE TABLE versions (bug INTEGER, version TEXT, status INTEGER NOT NULL DEFAULT 1, PRIMARY KEY(bug, version))')
        db.execute('CREATE INDEX versions_bug_index ON versions (bug)')
        db.execute('CREATE INDEX versions_version_index ON versions (version)')
        db.execute('PRAGMA user_version=4')
        db.commit()
        version = 4

    if version < 0 or version > 4:
        logging.error("Unknown database version " + str(version))
        sys.exit(1)

    if version < 2:
        logging.info("Upgrading database to version 2...")
        db.execute('ALTER TABLE bugs ADD COLUMN deadline INTEGER')
        db.execute('ALTER TABLE bugs ADD COLUMN progress INTEGER')
        db.execute('ALTER TABLE bugs ADD COLUMN milestone TEXT')
        db.execute('PRAGMA user_version=2')
        version = 2
        db.commit()

    if version < 3:
        logging.info("Upgrading database to version 3...")
        db.execute('CREATE TABLE links (a INTEGER, b INTEGER, type INTEGER, PRIMARY KEY(a, b), FOREIGN KEY(a) REFERENCES bugs(id), FOREIGN KEY(b) REFERENCES bugs(id))')
        db.execute('CREATE INDEX links_a_index ON links (a)')
        db.execute('CREATE INDEX links_b_index ON links (b)')
        db.execute('INSERT INTO links (a, b, type) SELECT a, b, 1 FROM merges')
        db.execute('DROP TABLE merges')
        db.execute('PRAGMA user_version=3')
        version = 3
        db.commit()

    if version < 4:
        logging.info("Upgrading database to version 4...")
        for i in db.execute('SELECT bug, key FROM messages WHERE key IS NOT NULL'):
            filename = glob.glob(os.path.join(maildir, str(i[0]), "cur", i[1] + "*"))[0]
            store(email.message_from_file(open(filename, "r")))
            os.remove(filename)
        for i in db.execute('SELECT id FROM bugs'):
            try:
                os.rmdir(os.path.join(maildir, str(i[0]), "cur"))
                os.rmdir(os.path.join(maildir, str(i[0]), "new"))
                os.rmdir(os.path.join(maildir, str(i[0]), "tmp"))
                os.rmdir(os.path.join(maildir, str(i[0])))
            except:
                pass
        #db.execute('ALTER TABLE messages DROP COLUMN key')
        db.execute('DROP INDEX messages_key_index')
        db.execute('PRAGMA user_version=4')
        try:
            os.rmdir(os.path.join(maildir, "cur"))
            os.rmdir(os.path.join(maildir, "new"))
        except:
            pass
        version = 4
        db.commit()

    return db

def init(dir=None):
    global basedir, config, dbfile, maildir, hookdir, project, admin, respond_to_new, respond_to_reply
    global emailaddress, emailname, emailtemplates, smtphost, webroot, staticroot, webtemplates
    global db, mail, default_templates

    if not dir:
        dir = os.getenv("LIGHTBTS_DIR")

    if not dir:
        dir = os.getcwd()
        while dir:
            if os.access(os.path.join(dir, ".lightbts", "config"), os.F_OK):
                break
            parent = os.path.dirname(dir)
            if parent == dir:
                dir = None
            else:
                dir = parent
        if not dir:
            logging.error("No LightBTS instance found")
            sys.exit(1)
        dir = os.path.join(dir, ".lightbts")

    basedir = dir
    default_templates = os.path.join(os.path.split(__file__)[0], "templates")

    # Set default options

    config = configparser.SafeConfigParser()

    config.add_section('core')
    config.set('core', 'project', '')
    config.set('core', 'admin', '')
    config.set('core', 'messages', 'messages')
    config.set('core', 'hooks', 'hooks')
    config.set('core', 'index', 'index')
    config.set('core', 'respond-to-new', 'yes')
    config.set('core', 'respond-to-reply', 'yes')

    config.add_section('web')
    config.set('web', 'root', '/')
    config.set('web', 'static-root', '/')
    config.set('web', 'templates', 'templates')

    config.add_section('email')
    config.set('email', 'address', getpass.getuser() + '@' + platform.node())
    config.set('email', 'name', '')
    config.set('email', 'templates', 'templates')
    config.set('email', 'smtphost', 'localhost')

    config.add_section('cli')
    config.set('cli', 'editor', '')
    config.set('cli', 'pager', '')

    # Read the configuration file

    if not os.access(basedir, os.F_OK):
        os.makedirs(basedir)

    config.read(os.path.join(basedir, 'config'))

    # Core configuration
    dbfile = os.path.join(basedir, config.get('core', 'index'))
    maildir = os.path.join(basedir, config.get('core', 'messages'))
    hookdir = os.path.join(basedir, config.get('core', 'hooks'))
    project = config.get('core', 'project')
    admin = config.get('core', 'admin')
    respond_to_new = config.getboolean('core', 'respond-to-new')
    respond_to_reply = config.getboolean('core', 'respond-to-reply')

    # Email configuration
    emailaddress = config.get('email', 'address')
    emailname = config.get('email', 'name')
    if not emailname:
        emailname = project
    emailtemplates = os.path.join(basedir, config.get('email', 'templates'))
    smtphost = config.get('email', 'smtphost')

    # Web configuration
    webroot = config.get('web', 'root')
    staticroot = config.get('web', 'static-root')
    if not staticroot:
        staticroot = webroot
    webtemplates = os.path.join(basedir, config.get('web', 'templates'))

    # Open the mailbox
    mail = mailbox.Maildir(maildir)

    # Create the hooks directory
    try:
        os.mkdir(hookdir)
        # TODO: install hook templates
    except OSError:
        pass

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
    config.write(open(os.path.join(basedir, 'config'), 'w'))

def create_message(title, address, text, headers={}):
    global db, mail

    msg = email.MIMEText.MIMEText(text)

    msg['Subject'] = title
    msg['Message-ID'] = email.utils.make_msgid('LightBTS')
    msg['From'] = address
    msg['To'] = 'LightBTS'
    msg['Date'] = email.utils.formatdate()
    msg['User-Agent'] = 'LightBTS/' + __version__

    for key, value in headers.items():
        msg[key] = value

    msgid = email.utils.unquote(msg['Message-ID'])

    # Save message

    store(msg)

    # Store the message in the database

    try:
        db.execute("INSERT INTO messages (msgid, bug) values (?,?,?)", (msgid, 0));
    except sqlite3.IntegrityError:
        # TODO: throw something intelligent
        logging.error("Integrity error while creating a new message")
        return None

    return message(msgid, msg=msg)

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

def parse_metadata(bug, msg, msgstatus = None):
    text = None

    for part in msg.walk():
        if part.get_content_type() == "text/plain":
            text = part.get_payload()
            break

    if not text:
        return

    regex = re.compile("([A-Za-z-]+):[ \t]+(.*)")

    status = None
    severity = None
    title = None
    tags = []
    version = None
    found = []
    notfound = []
    fixed = []
    notfixed = []
    owner = None
    progress = None
    milestone = None
    deadline = None

    if msg['X-LightBTS-Status']:
        try:
            status = statusindex(msg['X-LightBTS-Status'])
        except ValueError:
            log += "Invalid status field.\n"

    if msg['X-LightBTS-Tag']:
        tags.append(msg['X-LightBTS-Tag'])

    for line in text.splitlines():
        match = regex.match(line)
        if not match:
            break

        key = match.group(1).lower()
        value = match.group(2).strip()

        if key == "status":
            if status:
                log += "Duplicate status field.\n"
            try:
                status = statusindex(value.lower())
            except ValueError:
                log += "Invalid status field.\n"

        elif key == "severity":
            if severity:
                log += "Duplicate severity field.\n"
            try:
                severity = severityindex(value.lower())
            except ValueError:
                log += "Invalid severity field.\n"

        elif key == "tags" or key == "tag":
            tags.append(value)

        elif key == "version":
            version = value

        elif key == "found":
            found.extend(value.split())

        elif key == "notfound":
            notfound.extend(value.split())

        elif key == "fixed":
            fixed.extend(value.split())

        elif key == "notfixed":
            notfixed.extend(value.split())

        elif key == "owner":
            if owner:
                log += "Duplicate owner field.\n"
            owner = value

        elif key == "progress":
            if value[-1] == '%':
                value = value[:-1]
            try:
                progress = int(value)
            except ValueError:
                log += "Invalid progress field.\n"

        elif key == "milestone":
            if milestone:
                log += "Duplicate milestone field.\n"
            milestone = value

        elif key == "deadline":
            if deadline:
                log += "Duplicate deadline field.\n"
            try:
                deadline = int(dateutil.parser.parse(value).strftime("%s"))
            except ValueError:
                log += "Invalid deadline field.\n"

        elif key == "title" or key == "topic":
            if title:
                log += "Duplicate title field.\n"
            title = value

        elif key in linktypes:
            others = [get_bug(s) for s in value.split()]
            bug.link(linktypeindex(key), *others)

        elif key[0:2] == 'un' and key[2:] in linktypes:
            others = [get_bug(s) for s in value.split()]
            bug.unlink(linktypeindex(key[2:]), *others)

    if status is not None:
        bug.set_status(status)

    if severity is not None:
        bug.set_severity(severity)

    if title:
        bug.set_title(title)

    if version:
        bug.set_version_status(version, bug.get_status())

    for version in notfound:
        bug.notfound(version)

    for version in found:
        bug.found(version)

    for version in notfixed:
        bug.notfixed(version)

    for version in fixed:
        bug.fixed(version)

    if progress is not None:
        bug.set_progress(progress)

    if milestone:
        bug.set_milestone(milestone)

    if deadline:
        bug.set_deadline(deadline)

    if owner:
        if owner == '-':
            bug.set_owner(None)
        else:
            bug.set_owner(owner)

    for tag in tags:
        bug.modify_tags(tag.split())

def forward_message(bug, msg):
    # Get email addresses of bug participants
    # TODO: handle -quiet, -maintonly, -submitter here.
    if quiet:
        return

    do = [admin]
    for i in db.execute("SELECT address FROM recipients WHERE bug=?", (bug._id,)):
        do.append(i[0])

    do = set(map((lambda x: x[1]), email.utils.getaddresses(do)))

    # Get all To: and Cc: addresses from the message

    dont = msg.get_all('From', []) + msg.get_all('To', []) + msg.get_all('Cc', [])
    dont = set(email.utils.getaddresses(dont))

    # Send a copy to those who didn't get the message yet

    do -= dont

    if do:
        smtp = smtplib.SMTP(smtphost)
        smtp.sendmail(msg['From'], do, msg.as_string())

def run_hook(name, filename, bug = None):
    if no_hooks:
        return True

    # Prepare environment variables
    env = os.environ.copy()
    env['LIGHTBTS_DIR'] = basedir
    env['MESSAGE_FILE'] = filename
    if bug:
        env['BUG_ID'] = str(bug)
    null = open('/dev/null', 'r+')
    try:
        process = subprocess.Popen([os.path.join('hooks', name)], cwd = basedir, env = env, stdin = null, stdout = null)
    except OSError:
        # If the hook is not startable, ignore the result.
        return True;

    return process.wait() == 0

def import_email(msg):
    # Don't allow messages with the X-LightBTS-Control header set

    if msg['X-LightBTS-Control']:
        logging.error("Denying import of message with X-LightBTS-Control header from " + msg['From'])
        return (None, None)

    # Handle missing Message-ID

    if not msg['Message-ID']:
        msg['Message-ID'] = email.utils.make_msgid('LightBTS')

    # Add a Received: header

    now = email.utils.formatdate()
    msg._headers.insert(0, ('Received', 'by localhost (LightBTS); ' + now))

    # Handle missing Date

    if not msg['Date']:
        msg['Date'] = now

    # Save message to new

    msgid = email.utils.unquote(msg['Message-ID'])
    parent = email.utils.unquote(msg['In-Reply-To'] or '')
    subject = msg['Subject']
    filename = store(msg)

    # Run the pre-index hook

    if not run_hook('pre-index', filename):
        logging.error("Pre-index script returned with an error.")
        return (None, None)

    # Store the message in the database

    try:
        db.execute("INSERT INTO messages (msgid, bug) values (?,?)", (msgid, 0));
    except sqlite3.IntegrityError:
        # TODO: throw something intelligent
        logging.warning("Ignoring duplicate message from " + msg['From'] + " with Message-ID " + msgid)
        return (None, None)

    # Can we match the message to an existing bug?

    bugno = 0
    new = False

    if parent:
        matches = db.execute("SELECT bug FROM messages WHERE msgid=?", (parent,))
        for i in matches:
            if i[0]:
                bugno = i[0]

    # Try finding one with a similar subject
    # TODO: more robust?

    if not bugno:
            matches = db.execute("SELECT id FROM bugs WHERE title LIKE ?", ('%' + subject,))
            for i in matches:
                if i[0]:
                    bugno = i[0]

    if not bugno:
        bugno = db.execute("INSERT INTO bugs (title) VALUES (?)", (subject,)).lastrowid
        new = True

    bug = get_bug(bugno)

    db.execute("UPDATE messages SET bug=? WHERE msgid=?", (bugno, msgid))

    db.execute("INSERT OR IGNORE INTO recipients (bug, address) VALUES (?, ?)", (bugno, msg['From']))

    # Email a copy to interested people

    forward_message(bug, msg)

    # Handle metadata

    parse_metadata(bug, msg)

    # Run the post-index hook

    run_hook('post-index', filename, bug = bugno)

    return (bug, new)

# TODO: mostly duplicates import_email, refactor.
# TODO: maybe we already know the bug number?
def update_index(filename):
    file = open(filename, 'r')
    msg = email.message_from_file(file)

    msgid = email.utils.unquote(msg['Message-ID'])
    parent = email.utils.unquote(msg['In-Reply-To'] or '')
    subject = msg['Subject']

    # Require a Message-ID

    if not msgid:
        logging.error("Missing Message-ID")
        return (None, None)

    # Store the message in the database

    try:
        db.execute("INSERT INTO messages (msgid, bug) values (?,?)", (msgid, 0));
    except sqlite3.IntegrityError:
        # TODO: throw something intelligent
        logging.warning("Ignoring duplicate message from " + msg['From'] + " with Message-ID " + msgid)
        return (None, None)

    # Check if this bug exists

    bugno = 0
    new = False

    if parent:
        matches = db.execute("SELECT bug FROM messages WHERE msgid=?", (parent,))
        for i in matches:
            if i[0]:
                bugno = i[0]

    # Try finding one with a similar subject
    # TODO: more robust?

    if not bugno:
            matches = db.execute("SELECT id FROM bugs WHERE title LIKE ?", ('%' + subject,))
            for i in matches:
                if i[0]:
                    bugno = i[0]

    if not bugno:
        bugno = db.execute("INSERT INTO bugs (title) VALUES (?)", (subject,)).lastrowid
        new = True

    bug = get_bug(bugno)

    db.execute("UPDATE messages SET bug=? WHERE msgid=?", (bugno, msgid))

    db.execute("INSERT OR IGNORE INTO recipients (bug, address) VALUES (?, ?)", (bugno, msg['From']))

    # Handle metadata

    parse_metadata(bug, msg)

    return (bug, new)

def record_msgid(bugno, msgid):
    db.execute("INSERT INTO messages (msgid, bug) values (?,?)", (msgid, bugno));
