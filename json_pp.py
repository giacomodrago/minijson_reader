#!/usr/bin/env python3

r"""Command-line tool to validate and pretty-print JSON

Usage::

    $ echo '{"json":"obj"}' | json_pp.py
    {
        "json": "obj"
    }

Validate json::

    $ echo '{ 1.2:3.4}' | json_pp.py
    Expecting property name: line 1 column 2 (char 2)

    $ json_pp.py -c sample.json | test_json_validity.py

Pretty printing::

    $ echo '{"name":null, "list":[{"key":1,"bool":false}]}' | json_pp.py
    {
       "name": null,
       "list": [
          {
             "key": 1,
             "bool": false
          }
       ]
    }

Compact encoding::

    $ echo '{ "bool":   false, "object":   { "key":  1, "flag":  true   } }' | json_pp.py -c -
    {"bool":false,"object":{"key":1,"flag":true}}

Pretty printing a compacted json file::

    $ json_pp.py -p sample.json
    {
       "bool": false,
       "object": {
          "key": 1,
          "flag": true
       }
    }

"""

import sys
import simplejson as json


def main():
    compact = False
    outfile = sys.stdout
    infile = sys.stdin
    if len(sys.argv) > 1:
        if "-c" in sys.argv:
            compact = True
        if len(sys.argv) == 3:
            fileName = sys.argv[-1]
            if fileName != "-":
                infile = open(fileName, 'r')
        elif len(sys.argv) == 4:
            infile = open(sys.argv[-2], 'r')
            outfile = open(sys.argv[-1], 'w')
        else:
            raise SystemExit(sys.argv[0] + " {-c|-p} [infile [outfile]]")

    with infile:
        try:
            obj = json.load(infile,
                            object_pairs_hook=json.OrderedDict,
                            use_decimal=True)
        except ValueError:
            raise SystemExit(sys.exc_info()[1])

    with outfile:
        if (compact):
            # Compact encoding::
            json.dump(obj, outfile, sort_keys=False, separators=(',', ':'))
        else:
            # Pretty printing::
            json.dump(obj, outfile, sort_keys=False, indent='   ', use_decimal=True)

        outfile.write('\n')


if __name__ == '__main__':
    main()
