# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

def build(bld):
    module = bld.create_ns3_module('ospf', ['core', 'internet', 'applications', 'point-to-point', 'mobility', 'internet-apps'])
    module.source = [
        'model/ospf-app.cc',
        'model/ospf-interface.cc',
        'model/ospf-header.cc',
        'model/lsa-header.cc',
        'model/router-lsa.cc',
        'helper/ospf-app-helper.cc',
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
        'model/ospf-app.h',
        'model/ospf-interface.h',
        'model/ospf-header.h',
        'model/lsa-header.h',
        'model/router-lsa.h',
        'helper/ospf-app-helper.h',
        'helper/ospf-packet-helper.h'
        ]

    if bld.env['ENABLE_EXAMPLES']:
        bld.recurse('examples')

    bld.ns3_python_bindings()

