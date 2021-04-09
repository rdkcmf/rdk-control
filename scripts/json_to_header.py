#!/usr/bin/python
##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
import os, sys, json
import argparse

def process_cmd_line():
   parser = argparse.ArgumentParser()
   parser.add_argument('-i', '--input',      action='store',                      help="Input json file", required=True)
   parser.add_argument('-o', '--output',     action='store',                      help="Output header file", required=True)
   parser.add_argument('-m', '--max_level',  action='store', type=int, default=0, help="Maximum level to process sub objects")
   parser.add_argument('-v', '--valid_keys', action='store',                      help="Maximum level to process sub objects")
   
   return(parser.parse_args())

header = '\
/*\n\
* If not stated otherwise in this file or this components license file the\n\
* following copyright and licenses apply:\n\
* Copyright 2020 RDK Management\n\
*\n\
* Licensed under the Apache License, Version 2.0 (the "License");\n\
* you may not use this file except in compliance with the License.\n\
* You may obtain a copy of the License at\n\
*\n\
* http://www.apache.org/licenses/LICENSE-2.0\n\
*\n\
* Unless required by applicable law or agreed to in writing, software\n\
* distributed under the License is distributed on an "AS IS" BASIS,\n\
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n\
* See the License for the specific language governing permissions and\n\
* limitations under the License.\n\
*/\n\
\n\
#ifndef _{0}_\n\
#define _{0}_\n\
\n\
// IMPORTANT NOTE:  The definitions in this file are automatically generated from {1}. DO NOT modify or commit this file.\n\n\
'

footer = '\n#endif\n'

def main():
   args = process_cmd_line()

   print("Convert {} to {}".format(args.input, args.output))
   
   if not os.path.isfile(args.input):
      print("json input file not found <{}>".format(args.input))
      sys.exit(1)

   json_in = json_load(args.input)

   print("Open output file <{}>".format(args.output))
   fh = open(args.output, 'w')
   
   fn_caps = os.path.basename(args.output).upper().replace('.', '_')
   
   fh.write(header.format(fn_caps, os.path.basename(args.input)))
   
   if args.valid_keys is None:
      valid_keys = None
   else:
      valid_keys = args.valid_keys.split(',')
   
   json_to_header_body(json_in, fh, max_level=args.max_level, valid_keys=valid_keys)
   
   fh.write(footer)
   fh.close();
   print("Close output file <{}>".format(args.output))
   return 0

def json_load(filename):
   with open(filename, 'r') as f:
      data = json.load(f)
   return data

def json_to_header_body(json_in, fh, prefix="", level=0, max_level=0, valid_keys=None):
   level += 1
   for key in json_in:
      if isinstance(json_in[key], dict):
         str_name = '#define JSON_OBJ_NAME_{}{}'.format(prefix, key.upper())
         fh.write('{: <82} \"{}\"\n'.format(str_name, key))
         if valid_keys is not None:
            if key in valid_keys:
               json_to_header_body(json_in[key], fh, prefix + "{}_".format(key.upper()), level, max_level)
         elif max_level == 0 or level < max_level: # Don't recurse for X level keys and beyond
            json_to_header_body(json_in[key], fh, prefix + "{}_".format(key.upper()), level, max_level)
      elif isinstance(json_in[key], bool):
         str_name  = '#define JSON_BOOL_NAME_{}{}'.format(prefix, key.upper())
         str_value = '#define JSON_BOOL_VALUE_{}{}'.format(prefix, key.upper())
         fh.write('{: <82} \"{}\"\n'.format(str_name, key)) 
         fh.write('{: <82} ({})\n'.format(str_value, str(json_in[key]).lower()))
      elif isinstance(json_in[key], float): 
         str_name  = '#define JSON_FLOAT_NAME_{}{}'.format(prefix, key.upper())
         str_value = '#define JSON_FLOAT_VALUE_{}{}'.format(prefix, key.upper())
         fh.write('{: <82} \"{}\"\n'.format(str_name, key)) 
         fh.write('{: <82} ({})\n'.format(str_value, json_in[key]))
      elif isinstance(json_in[key], int): 
         str_name  = '#define JSON_INT_NAME_{}{}'.format(prefix, key.upper())
         str_value = '#define JSON_INT_VALUE_{}{}'.format(prefix, key.upper())
         fh.write('{: <82} \"{}\"\n'.format(str_name, key)) 
         fh.write('{: <82} ({})\n'.format(str_value, json_in[key]))
      elif isinstance(json_in[key], str):
         str_name  = '#define JSON_STR_NAME_{}{}'.format(prefix, key.upper())
         str_value = '#define JSON_STR_VALUE_{}{}'.format(prefix, key.upper())
         fh.write('{: <82} \"{}\"\n'.format(str_name, key)) 
         fh.write('{: <82} \"{}\"\n'.format(str_value, json_in[key]))
      elif isinstance(json_in[key], list) or isinstance(json_in[key], tuple):
         str_name  = '#define JSON_ARRAY_NAME_{}{}'.format(prefix, key.upper())
         fh.write('{: <82} \"{}\"\n'.format(str_name, key))
         for value in json_in[key]:
            if isinstance(value, str) or isinstance(value, unicode):
               str_value = '#define JSON_ARRAY_VAL_STR_{}{}'.format(prefix, value.upper())
               fh.write('{: <82} \"{}\"\n'.format(str_value, value))
            else:
               raise TypeError("unhandled array instance type <{}> key <{}>".format(type(json_in[key]), key))
      else:
         raise TypeError("unhandled instance type <{}> key <{}>".format(type(json_in[key]), key))

if __name__ == "__main__":
   try:
      main()
   except Exception as e:
      print(e)
      sys.exit(1)
