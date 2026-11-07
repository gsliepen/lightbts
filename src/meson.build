configuration = configuration_data()
configuration.set('VERSION', meson.project_version())
version_header = configure_file(
	input: 'version.hpp.in',
	output: 'version.hpp',
	configuration: configuration,
)

gentemplates = executable('gentemplates', 'gentemplates.cpp', native: true)

template_files = files(
	'../templates/bug.html',
	'../templates/lightbts.css',
	'../templates/main.html',
	'../templates/new.txt',
	'../templates/reply.txt',
)

templates = custom_target('gen-templates',
	input: template_files,
	output: 'templates.inl',
	capture: true,
	command: [gentemplates, 'embedded_templates', '@INPUT@'],
)

lightbts_core = static_library('lightbts-core',
	'config.cpp',
	'lightbts.cpp',
	'process.cpp',
	'pseudoheaders.cpp',
	templates,
	dependencies: [blake2, mimesis, sqlite3],
)

core = declare_dependency(
	link_with: lightbts_core,
	include_directories: include_directories('.'),
	dependencies: [mimesis, sqlite3],
)

lbts = executable('lbts',
	'action.cpp',
	'cli.cpp',
	'compose.cpp',
	'create.cpp',
	'edit.cpp',
	'import.cpp',
	'list.cpp',
	'pager.cpp',
	'reply.cpp',
	'show.cpp',
	dependencies: core,
	install: true,
)
