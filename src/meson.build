gentemplates = executable('gentemplates',
	'gentemplates.cpp',
	dependencies: [
		boost,
	],
)

templates = custom_target('gen-templates',
	input: [
		'../templates/bug.html',
		'../templates/lightbts.css',
		'../templates/main.html',
		'../templates/new.txt',
		'../templates/reply.txt'
	],
	output: 'templates.inl',
	capture: true,
	command: [gentemplates, '@INPUT@'],
)

executable('lbts',
	'action.cpp',
	'cli.cpp',
	'config.cpp',
	'create.cpp',
	'edit.cpp',
	'import.cpp',
	'lightbts.cpp',
	'list.cpp',
	'pager.cpp',
	'reply.cpp',
	'show.cpp',
	templates,
	dependencies: [
		blake2,
		boost,
		fmtlib,
		mimesis,
		sqlite3,
	],
	install: true
)
