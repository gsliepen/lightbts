shell_tests = ['basic', 'config', 'fields', 'ids', 'create', 'import', 'reply', 'list', 'show', 'action', 'hooks', 'reindex', 'fsck']

foreach name: shell_tests
	test(name, files(name + '.test'), depends: lbts, workdir: meson.project_build_root())
endforeach

unit_tests = {
	'sqlite3': core,
	'pseudoheaders': core,
	'config': core,
	'fields': core,
	'fsck': core,
}

if build_web
	unit_tests += {'web-handler': web}
	test('web', files('web.test'), depends: lbts_web, workdir: meson.project_build_root())
endif

foreach name, dependency: unit_tests
	test(name + '-unit', executable(name.underscorify() + '_test', name.underscorify() + '_test.cpp', dependencies: dependency))
endforeach
