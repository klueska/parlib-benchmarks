#!/usr/bin/env python
####!/usr/bin/arch -i386 python2.7
#
# Author: Kevin Klues <klueska@cs.berkeley.edu>

import sys
from argparse import ArgumentParser
from generate_graphs import generate_graphs

##########################################################################
# Define some argument parameters used by commands in the command parser #
##########################################################################
arg_help_strings = {
  'input-folder' : "Name of the input folder where the *.dat files " \
                 + "generated by run-fixedwork reside.",
  'output-folder' : "Name of the output folder where all generated graphs " \
                  + "will be placed." \
}
arg_params = {
  'input-folder' : {
    'args'   : ['-i', '--input-folder'],
    'kwargs' : {
      'dest'    : 'input_folder',
      'metavar' : 'INPUT_FOLDER',
      'required' : True,
      'help'    : arg_help_strings['input-folder']
    }
  },
  'output-folder' : {
    'args'   : ['-o', '--output-folder'],
    'kwargs' : {
      'dest'    : 'output_folder',
      'metavar' : 'OUTPUT_FOLDER',
      'required' : True,
      'help'    : arg_help_strings['output-folder']
    }
  },
}


#######################################################
# Define the commands processed by the command parser #
#######################################################
cmd_help_strings = {
  'generate-graphs' : "Generate all graphs from the INPUT_FOLDER and place " \
                    + "them in OUTPUT_FOLDER." \
}
commands = [
  {
    'name'        : 'generate-graphs',
    'description' : cmd_help_strings['generate-graphs'],
    'arg_params'  : ['input-folder', 'output-folder'],
    'func'        : 'generate_graphs',
  },
]

#############################
# Define the command parser #
#############################
class CommandParser(ArgumentParser):
  def __init__(self, valid_arg_params, *args, **kwargs):
    super(CommandParser, self).__init__(*args, **kwargs)
    self.subparsers = self.add_subparsers(
      dest="command",
      title="subcommands",
      description="Run '%s " % sys.argv[0] \
                 +"<command> -h' for more information on each command"
    )
    self.subparsers._parser_class = ArgumentParser
    self.arg_params = valid_arg_params
    self.commands = {}

  def add_command(self, cmd):
    name = cmd['name']
    cmd_params =  {
      'args'   : [name],
      'kwargs' : {
        'description' : cmd['description'],
        'help'        : cmd['description'],
      }
    }
    self.commands[name] = self.subparsers.add_parser( *cmd_params['args'],
                                                     **cmd_params['kwargs'])
    self.commands[name].set_defaults(func=eval(cmd['func']))
    for p in cmd['arg_params']:
      self.commands[name].add_argument( *self.arg_params[p]['args'],
                                       **self.arg_params[p]['kwargs'])

##################################################################
# Create the command parser with all defined commands and run it #
##################################################################
parser = CommandParser(arg_params)
for cmd in commands:
  parser.add_command(cmd)
args = parser.parse_args()
args.func(args)

