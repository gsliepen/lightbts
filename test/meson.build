shell_tests = ['basic', 'config', 'create', 'import', 'reply', 'list', 'show', 'action', 'hooks']

foreach name: shell_tests
	test(name, files(name + '.test'), depends: lbts, workdir: meson.project_build_root())
endforeach

unit_tests = {
	'sqlite3': core,
	'pseudoheaders': core,
	'config': core,
}

foreach name, dependency: unit_tests
	test(name + '-unit', executable(name.underscorify() + '_test', name.underscorify() + '_test.cpp', dependencies: dependency))
endforeach
