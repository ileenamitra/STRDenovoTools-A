/*
Copyright (C) 2017 Melissa Gymrek <mgymrek@ucsd.edu>

This file is part of STRDenovoTools.

STRDenovoTools is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

STRDenovoTools is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with STRDenovoTools.  If not, see <http://www.gnu.org/licenses/>.
*/

using namespace std;

#include <getopt.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "src/common.h"
#include "src/denovo_scanner.h"
#include "src/locus_inspector.h"
#include "src/options.h"
#include "src/pedigree.h"
#include "src/vcf_reader.h"

bool file_exists(const std::string& path){
  return (access(path.c_str(), F_OK) != -1);
}

void show_help() {
  std::stringstream help_msg;
  help_msg << "\nExamineDenovo"
	   << " --str-vcf <STR VCF file>"
	   << " --fam <pedigree file>"
	   << " --locus <chrom:start>"
	   << " --family <STR>\n"
	   << "****** Other options *********\n"
	   << "-h,--help      display this help screen\n"
	   << "-v,--verbose   print out useful progress messages\n"
	   << "--version      print out the version of this software\n\n";
  cerr << help_msg.str();
  exit(1);
}

void parse_commandline_options(int argc, char* argv[], Options* options) {
  enum LONG_OPTIONS {
    OPT_FAM,
    OPT_FAMILY,
    OPT_HELP,
    OPT_LOCUS,
    OPT_STRVCF,
    OPT_VERBOSE,
    OPT_VERSION,
  };
  static struct option long_options[] = {
    {"fam", 1, 0, OPT_FAM},
    {"family", 1, 0, OPT_FAMILY},
    {"help", 0, 0, OPT_HELP},
    {"locus", 1, 0, OPT_LOCUS},
    {"strvcf", 1, 0, OPT_STRVCF},
    {"verbose", 0, 0, OPT_VERBOSE},
    {"version", 0, 0, OPT_VERSION},
    {NULL, no_argument, NULL, 0},
  };
  int ch;
  int option_index = 0;
  ch = getopt_long(argc, argv, "hv?",
                   long_options, &option_index);
  while (ch != -1) {
    switch (ch) {
    case OPT_FAM:
      options->famfile = optarg;
      break;
    case OPT_FAMILY:
      options->family = optarg;
      break;
    case OPT_HELP:
    case 'h':
      show_help();
    case OPT_LOCUS:
      options->locus = optarg;
      break;
    case OPT_STRVCF:
      options->strvcf = optarg;
      break;
    case OPT_VERBOSE:
    case 'v':
      options->verbose++;
      break;
    case OPT_VERSION:
      cerr << _GIT_VERSION << endl;
      exit(0);
    case '?':
      show_help();
    default:
      show_help();
    };
    ch = getopt_long(argc, argv, "hv?",
		     long_options, &option_index);
  };
  // Leftover arguments are errors
  if (optind < argc) {
    PrintMessageDieOnError("Unnecessary leftover arguments", M_ERROR);
  }
  // Perform other error checking
  if (options->strvcf.empty()) {
    PrintMessageDieOnError("No --strvcf specified", M_ERROR);
  }
  if (options->famfile.empty()) {
    PrintMessageDieOnError("No --fam option specified", M_ERROR);
  }
  if (options->family.empty()) {
    PrintMessageDieOnError("No --family option specified", M_ERROR);
  }
}

int main(int argc, char* argv[]) {
  // Set up
  Options options;
  parse_commandline_options(argc, argv, &options);

  // Load STR VCF file
  if (!file_exists(options.strvcf)) {
    PrintMessageDieOnError("STR vcf file " + options.strvcf + " does not exist.", M_ERROR);
  }
  if (!file_exists(options.strvcf + ".tbi")) {
    PrintMessageDieOnError("No .tbi index found for " + options.strvcf, M_ERROR);
  }
  VCF::VCFReader strvcf(options.strvcf);
  std::set<std::string> str_samples(strvcf.get_samples().begin(), strvcf.get_samples().end());
  if (!options.region.empty()) {
    strvcf.set_region(options.region);
  }

  // Extract nuclear families for samples with data
  if (!file_exists(options.famfile)) {
    PrintMessageDieOnError("FAM file " + options.famfile + " does not exist.", M_ERROR);
  }
  PedigreeSet pedigree_set;
  if (!pedigree_set.ExtractFamilies(options.famfile, str_samples, options.require_num_children)) {
    PrintMessageDieOnError("Error extracting families from the pedigree.", M_ERROR);
  }
  pedigree_set.PrintStatus();

  // Look for this family
  size_t family_index;
  if (!pedigree_set.GetFamilyIndex(options.family, &family_index)) {
    PrintMessageDieOnError("Could not find family " + options.family, M_ERROR);
  }
  NuclearFamily myfam = pedigree_set.get_families()[family_index];
  // Look for the variant
  strvcf.set_region(options.locus);
  std::vector<std::string> items;
  split_by_delim(options.locus, ':', items);
  if (items.size() != 2) {
    PrintMessageDieOnError("Invalid locus format", M_ERROR);
  }
  std::string chrom = items[0];
  int32_t start = atoi(items[1].c_str());
  VCF::Variant str_variant;
  int32_t str_start;
  bool found_variant = false;
  while (strvcf.get_next_variant(&str_variant, false)) {
    str_variant.get_INFO_value_single_int("START", str_start);
    if (str_variant.get_chromosome() == chrom && str_start == start) {
      found_variant = true;
      break;
    }
  }
  if (!found_variant) {
    PrintMessageDieOnError("Couldn't find locus in the VCF", M_ERROR);
  }
  // Get info on this locus/family
  stringstream ss;
  ss << "Examining locus " << options.locus << " in family " << options.family;
  PrintMessageDieOnError(ss.str(), M_PROGRESS);
  LocusInspector locus_inspector;
  locus_inspector.Inspect(str_variant, myfam.get_mother(),
			  strvcf.get_sample_index(myfam.get_mother()), 
			  pedigree_set.get_families(),
			  "mother", PT_MISSING, false);
  locus_inspector.Inspect(str_variant, myfam.get_father(),
			  strvcf.get_sample_index(myfam.get_father()),
			  pedigree_set.get_families(),
			  "father", PT_MISSING, false);
  for (size_t i = 0; i < myfam.num_children(); i++) {
    locus_inspector.Inspect(str_variant, myfam.get_children()[i],
			    strvcf.get_sample_index(myfam.get_children()[i]),
			    pedigree_set.get_families(),
			    "child", myfam.GetChildrenStatus()[i], false);
  }
  str_variant.destroy_record();
  return 0;
}
