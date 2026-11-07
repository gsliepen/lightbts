LightBTS roadmap
----------------

LightBTS should work well in three situations:

* As a personal issue tracker: run `lbts init` inside a project directory, and
  use the command line interface.
* As a public issue tracker, with an email and a web interface.
* As a general issue tracker, not only for bugs, that could replace Jira for
  teams that don't need everything Jira offers.

The architecture already suits all three: messages are the source of truth,
the SQLite index can be rebuilt from them, and changes to tickets are
pseudo-headers in messages. What is mostly needed is to get a few foundations
right before building more features on top of them.

## 1. Foundations

### Reindexing and consistency checks

The design promises that the index can always be rebuilt from the messages,
but there is no command to do that yet. Add `lbts reindex` and `lbts fsck`:

- Rebuild the index by importing all messages in the order they were
  originally received, using the Received header added by LightBTS, and
  falling back to the Date header.
- Check that every message in the index exists, and that every message file
  is in the index.

This keeps the messages the single source of truth, turns changes to the
database schema into a matter of rebuilding the index instead of writing
migrations, and is the basis for synchronizing instances.

### A generic field model

Status and severity are hard-coded, and mirror the Debian BTS. Since
pseudo-headers are already key/value pairs, let the configuration define the
fields of a ticket instead:

- Each field has a name, a type (enumeration, text, user, date, number), the
  allowed values and a default.
- The index stores them in a generic `fields(ticket, name, value)` table.
- Statuses and ticket types (bug, feature, task, ...) become configurable.

This is what turns a bug tracker into a general issue tracker, and it is much
cheaper to do now than after more features depend on the hard-coded fields.

## 2. Personal issue tracker

- `lbts search`, using SQLite's FTS5 for full-text search of titles and
  messages.
- Output that is easy to use from scripts, for example `lbts list --format`
  and `--json`, sorting, and saved queries.
- Shell completion, `lbts edit`, and saving attachments to disk.

### Storing tickets in git

The messages can be stored in the git repository of the project itself,
without mixing them with the project's history:

- Store the LightBTS data in an orphan branch, which shares no history with
  the other branches of the repository.
- Check out that branch as a second worktree in `.lightbts`, which the
  project's own branches ignore.
- Only the messages, and possibly the configuration and templates, are
  tracked. The index is ignored, and is rebuilt with `lbts reindex` after a
  pull, for example from a git hook.
- Hooks must not be tracked, or at least must not run automatically after
  cloning, since that would let anyone who can push to the branch run code on
  the machines of everyone who pulls from it.
- `lbts init` could set this up, and detect an existing branch in a cloned
  repository.

Open problem: ticket numbers are assigned locally, so two clones can create
different tickets with the same number. Tickets should be identified by the
Message-ID of their first message, with numbers as a local convenience. How to
keep numbers stable when synchronizing needs to be decided before people
start relying on them, for example by recording number assignments in
messages, and resolving conflicts deterministically.

## 3. Public issue tracker

### Email interface

Add `lbts-email`:

- Receive messages from the mail server, as a delivery agent reading from
  stdin, or via LMTP.
- Send messages via `sendmail`: forward messages to everyone who participates
  in a ticket (the `recipients` table already exists), and send
  acknowledgements using the `new.txt` and `reply.txt` templates.
- Put `Bug#N` in the subjects of messages sent by LightBTS, and handle
  `Auto-Submitted` and similar headers to prevent mail loops.
- Support both a dedicated address for tickets, and a quieter mode for use on
  a mailing list, as described in `doc/goals.mdwn`.

### Identity and permissions

Currently, anyone who can send a message can change any field of any ticket.
For a public tracker, LightBTS needs to know who sent a message, and what they
are allowed to do.

- On the web, authenticate users with OpenID Connect, or by trusting
  `REMOTE_USER` set by a reverse proxy.
- For email, authenticate messages with PGP signatures. Unsigned messages can
  still be accepted, but only from verified addresses and with fewer
  permissions.
- Bind the two together. For example, after logging in on the web, a user
  registers their PGP key, and proves ownership of the key and their email
  address by sending a signed message containing a token shown on the web
  page. From then on, signed email and web actions are attributed to the same
  identity.
- Map identities to roles in the configuration, and restrict which
  pseudo-headers and values each role may set. For example, only maintainers
  may change the severity or close someone else's ticket.
- Quarantine messages from new senders until their address is verified.
- Use the existing `spam` column, and add commands to mark messages as spam.

### Writing via the web interface

Once identities exist, add forms to create tickets, reply, and change fields.
These compose a message in the same way as the command line interface, so
they need no separate code path for changing tickets. Protect them against
cross-site request forgery.

### API and feeds

- A JSON API for the same data as the web pages.
- Atom feeds for the ticket list and for individual tickets.

## 4. General issue tracking

Matching Jira feature for feature is not realistic, nor the goal. The goal is
a tracker that is plain text, works offline and is email native, for teams
that don't need the rest. Building on the generic field model:

- Configurable workflows: which statuses exist, which transitions between
  them are allowed, and who may make them.
- A board view in the web interface, with a column per status. Moving a
  ticket to another column sends an action message.
- Multiple projects or components, and links between tickets in different
  instances.
- Simple reports: open tickets over time, per owner, or per milestone.

## Suggested order

1. Reindexing and consistency checks.
2. The generic field model, with configurable statuses and ticket types.
3. Search, and output that is easy to use from scripts.
4. Storing tickets in git.
5. The email interface.
6. Identity and permissions, then writing via the web interface.
7. Synchronizing instances, boards and reports.

## Related projects

These projects solve similar problems, and are worth studying:

- [SourceHut todo](https://man.sr.ht/todo.sr.ht/): a public issue tracker
  with email and web interfaces.
- [debbugs](https://www.debian.org/Bugs/): the Debian BTS, which the design of
  LightBTS is based on.
- [git-bug](https://github.com/git-bug/git-bug): issues stored in git, and
  synchronized between clones.
- [Fossil](https://fossil-scm.org/): its ticket system has configurable
  ticket fields.
