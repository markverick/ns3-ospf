# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

def build(bld):
    module = bld.create_ns3_module('ospf', ['core', 'internet', 'applications', 'point-to-point', 'mobility', 'internet-apps'])
    module.source = [
        'ospf-app-helper.cc',
        'ospf-app.cc',
        'ospf-interface.cc'
        ]

    module_test = bld.create_ns3_module_test_library('ospf')
    module_test.source = [
        ]
    # Tests encapsulating example programs should be listed here
    if (bld.env['ENABLE_EXAMPLES']):
        module_test.source.extend([
    ])

    headers = bld(features='ns3header')
    headers.module = 'ospf'
    headers.source = [
        'ospf-app-helper.h',
        'ospf-app.h',
        'ospf-interface.h',
        'ospf-packet-helper.h'
        ]

    if bld.env['ENABLE_EXAMPLES']:
        bld.recurse('examples')

    bld.ns3_python_bindings()

