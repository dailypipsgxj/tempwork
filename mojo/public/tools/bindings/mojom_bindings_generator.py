#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The frontend for the Mojo bindings system."""


import argparse
import imp
import json
import os
import pprint
import re
import sys

# Disable lint check for finding modules:
# pylint: disable=F0401

def _GetDirAbove(dirname):
  """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
  path = os.path.abspath(__file__)
  while True:
    path, tail = os.path.split(path)
    assert tail
    if tail == dirname:
      return path

# Manually check for the command-line flag. (This isn't quite right, since it
# ignores, e.g., "--", but it's close enough.)
if "--use_bundled_pylibs" in sys.argv[1:]:
  sys.path.insert(0, os.path.join(_GetDirAbove("mojo"), "third_party"))

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "pylib"))

from mojom.error import Error
import mojom.fileutil as fileutil
from mojom.generate.data import OrderedModuleFromData
from mojom.generate import template_expander
from mojom.parse.parser import Parse
from mojom.parse.translate import Translate


_BUILTIN_GENERATORS = {
  "c++": "mojom_cpp_generator.py",
  "javascript": "mojom_js_generator.py",
  "java": "mojom_java_generator.py",
}


def LoadGenerators(generators_string):
  if not generators_string:
    return []  # No generators.

  script_dir = os.path.dirname(os.path.abspath(__file__))
  generators = {}
  for generator_name in [s.strip() for s in generators_string.split(",")]:
    language = generator_name.lower()
    if language in _BUILTIN_GENERATORS:
      generator_name = os.path.join(script_dir, "generators",
                                    _BUILTIN_GENERATORS[language])
    else:
      print "Unknown generator name %s" % generator_name
      sys.exit(1)
    generator_module = imp.load_source(os.path.basename(generator_name)[:-3],
                                       generator_name)
    generators[language] = generator_module
  return generators


def MakeImportStackMessage(imported_filename_stack):
  """Make a (human-readable) message listing a chain of imports. (Returned
  string begins with a newline (if nonempty) and does not end with one.)"""
  return ''.join(
      reversed(["\n  %s was imported by %s" % (a, b) for (a, b) in \
                    zip(imported_filename_stack[1:], imported_filename_stack)]))


class RelativePath(object):
  """Represents a path relative to the source tree."""
  def __init__(self, path, source_root):
    self.path = path
    self.source_root = source_root

  def relative_path(self):
    return os.path.relpath(os.path.abspath(self.path),
                           os.path.abspath(self.source_root))


def FindImportFile(rel_dir, file_name, search_rel_dirs):
  """Finds |file_name| in either |rel_dir| or |search_rel_dirs|. Returns a
  RelativePath with first file found, or an arbitrary non-existent file
  otherwise."""
  for rel_search_dir in [rel_dir] + search_rel_dirs:
    path = os.path.join(rel_search_dir.path, file_name)
    if os.path.isfile(path):
      return RelativePath(path, rel_search_dir.source_root)
  return RelativePath(os.path.join(rel_dir.path, file_name),
                      rel_dir.source_root)


class MojomProcessor(object):
  def __init__(self, should_generate):
    self._should_generate = should_generate
    self._processed_files = {}
    self._parsed_files = {}
    self._typemap = {}

  def LoadTypemaps(self, typemaps):
    # Support some very simple single-line comments in typemap JSON.
    comment_expr = r"^\s*//.*$"
    def no_comments(line):
      return not re.match(comment_expr, line)
    for filename in typemaps:
      with open(filename) as f:
        typemaps = json.loads("".join(filter(no_comments, f.readlines())))
        for language, typemap in typemaps.iteritems():
          language_map = self._typemap.get(language, {})
          language_map.update(typemap)
          self._typemap[language] = language_map

  def ProcessFile(self, args, remaining_args, generator_modules, filename):
    self._ParseFileAndImports(RelativePath(filename, args.depth),
                              args.import_directories, [])

    return self._GenerateModule(args, remaining_args, generator_modules,
        RelativePath(filename, args.depth))

  def _GenerateModule(self, args, remaining_args, generator_modules,
                      rel_filename):
    # Return the already-generated module.
    if rel_filename.path in self._processed_files:
      return self._processed_files[rel_filename.path]
    tree = self._parsed_files[rel_filename.path]

    dirname, name = os.path.split(rel_filename.path)
    mojom = Translate(tree, name)
    if args.debug_print_intermediate:
      pprint.PrettyPrinter().pprint(mojom)

    # Process all our imports first and collect the module object for each.
    # We use these to generate proper type info.
    for import_data in mojom['imports']:
      rel_import_file = FindImportFile(
          RelativePath(dirname, rel_filename.source_root),
          import_data['filename'], args.import_directories)
      import_data['module'] = self._GenerateModule(
          args, remaining_args, generator_modules, rel_import_file)

    module = OrderedModuleFromData(mojom)

    # Set the path as relative to the source root.
    module.path = rel_filename.relative_path()

    # Normalize to unix-style path here to keep the generators simpler.
    module.path = module.path.replace('\\', '/')

    if self._should_generate(rel_filename.path):
      for language, generator_module in generator_modules.iteritems():
        generator = generator_module.Generator(
            module, args.output_dir, typemap=self._typemap.get(language, {}),
            variant=args.variant, bytecode_path=args.bytecode_path,
            for_blink=args.for_blink,
            use_new_wrapper_types=args.use_new_wrapper_types)
        filtered_args = []
        if hasattr(generator_module, 'GENERATOR_PREFIX'):
          prefix = '--' + generator_module.GENERATOR_PREFIX + '_'
          filtered_args = [arg for arg in remaining_args
                           if arg.startswith(prefix)]
        generator.GenerateFiles(filtered_args)

    # Save result.
    self._processed_files[rel_filename.path] = module
    return module

  def _ParseFileAndImports(self, rel_filename, import_directories,
      imported_filename_stack):
    # Ignore already-parsed files.
    if rel_filename.path in self._parsed_files:
      return

    if rel_filename.path in imported_filename_stack:
      print "%s: Error: Circular dependency" % rel_filename.path + \
          MakeImportStackMessage(imported_filename_stack + [rel_filename.path])
      sys.exit(1)

    try:
      with open(rel_filename.path) as f:
        source = f.read()
    except IOError as e:
      print "%s: Error: %s" % (e.rel_filename.path, e.strerror) + \
          MakeImportStackMessage(imported_filename_stack + [rel_filename.path])
      sys.exit(1)

    try:
      tree = Parse(source, rel_filename.path)
    except Error as e:
      full_stack = imported_filename_stack + [rel_filename.path]
      print str(e) + MakeImportStackMessage(full_stack)
      sys.exit(1)

    dirname = os.path.split(rel_filename.path)[0]
    for imp_entry in tree.import_list:
      import_file_entry = FindImportFile(
          RelativePath(dirname, rel_filename.source_root),
          imp_entry.import_filename, import_directories)
      self._ParseFileAndImports(import_file_entry, import_directories,
          imported_filename_stack + [rel_filename.path])

    self._parsed_files[rel_filename.path] = tree


def _Generate(args, remaining_args):
  if args.variant == "none":
    args.variant = None

  for idx, import_dir in enumerate(args.import_directories):
    tokens = import_dir.split(":")
    if len(tokens) >= 2:
      args.import_directories[idx] = RelativePath(tokens[0], tokens[1])
    else:
      args.import_directories[idx] = RelativePath(tokens[0], args.depth)
  generator_modules = LoadGenerators(args.generators_string)

  fileutil.EnsureDirectoryExists(args.output_dir)

  processor = MojomProcessor(lambda filename: filename in args.filename)
  processor.LoadTypemaps(set(args.typemaps))
  for filename in args.filename:
    processor.ProcessFile(args, remaining_args, generator_modules, filename)

  return 0


def _Precompile(args, _):
  generator_modules = LoadGenerators(",".join(_BUILTIN_GENERATORS.keys()))

  template_expander.PrecompileTemplates(generator_modules, args.output_dir)
  return 0



def main():
  parser = argparse.ArgumentParser(
      description="Generate bindings from mojom files.")
  parser.add_argument("--use_bundled_pylibs", action="store_true",
                      help="use Python modules bundled in the SDK")

  subparsers = parser.add_subparsers()
  generate_parser = subparsers.add_parser(
      "generate", description="Generate bindings from mojom files.")
  generate_parser.add_argument("filename", nargs="+",
                               help="mojom input file")
  generate_parser.add_argument("-d", "--depth", dest="depth", default=".",
                               help="depth from source root")
  generate_parser.add_argument("-o", "--output_dir", dest="output_dir",
                               default=".",
                               help="output directory for generated files")
  generate_parser.add_argument("--debug_print_intermediate",
                               action="store_true",
                               help="print the intermediate representation")
  generate_parser.add_argument("-g", "--generators",
                               dest="generators_string",
                               metavar="GENERATORS",
                               default="c++,javascript,java",
                               help="comma-separated list of generators")
  generate_parser.add_argument(
      "-I", dest="import_directories", action="append", metavar="directory",
      default=[],
      help="add a directory to be searched for import files. The depth from "
           "source root can be specified for each import by appending it after "
           "a colon")
  generate_parser.add_argument("--typemap", action="append", metavar="TYPEMAP",
                               default=[], dest="typemaps",
                               help="apply TYPEMAP to generated output")
  generate_parser.add_argument("--variant", dest="variant", default=None,
                               help="output a named variant of the bindings")
  generate_parser.add_argument(
      "--bytecode_path", type=str, required=True, help=(
          "the path from which to load template bytecode; to generate template "
          "bytecode, run %s precompile BYTECODE_PATH" % os.path.basename(
              sys.argv[0])))
  generate_parser.add_argument("--for_blink", action="store_true",
                               help="Use WTF types as generated types for mojo "
                               "string/array/map.")
  generate_parser.add_argument(
      "--use_new_wrapper_types", action="store_true",
      help="Map mojom array/map/string to STL (for chromium variant) or WTF "
      "(for blink variant) types directly.")
  generate_parser.set_defaults(func=_Generate)

  precompile_parser = subparsers.add_parser("precompile",
      description="Precompile templates for the mojom bindings generator.")
  precompile_parser.add_argument(
      "-o", "--output_dir", dest="output_dir", default=".",
      help="output directory for precompiled templates")
  precompile_parser.set_defaults(func=_Precompile)

  args, remaining_args = parser.parse_known_args()
  return args.func(args, remaining_args)


if __name__ == "__main__":
  sys.exit(main())
