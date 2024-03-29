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

#ifndef VCF_READER_H_
#define VCF_READER_H_

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "src/common.h"
#include "htslib/bgzf.h"
#include "htslib/tbx.h"
#include "htslib/vcf.h"

namespace VCF {

  class VCFReader;

  class Variant {
  private:
    bcf_hdr_t const * vcf_header_;
    VCFReader const * vcf_reader_;
    bcf1_t* vcf_record_;

    int num_samples_, num_missing_;
    std::vector<std::string> alleles_;
    std::vector<bool> missing_;
    std::vector<bool> phased_;
    std::vector<int> gt_1_, gt_2_;
  
    void extract_alleles();
    void extract_genotypes();

    std::map<int, int> gt_to_length_index_;
    std::vector<int> length_allele_sizes_; 
  public:
    Variant(){
      vcf_header_  = NULL;
      vcf_record_  = NULL;
      vcf_reader_  = NULL;
      num_samples_ = 0;
      num_missing_ = 0;
    }

    Variant(bcf_hdr_t* vcf_header, bcf1_t* vcf_record, VCFReader* vcf_reader, const bool& round){
      vcf_header_  = vcf_header;
      vcf_record_  = vcf_record;
      vcf_reader_  = vcf_reader;
      num_samples_ = bcf_hdr_nsamples(vcf_header_);
      bcf_unpack(vcf_record_, BCF_UN_ALL);
      extract_alleles();
      extract_genotypes();
      num_missing_ = 0;
      for (int i = 0; i < num_samples_; ++i)
	if (missing_[i])
	  ++num_missing_;
      build_alleles_by_length(round);
    }
    
    const std::vector<std::string>& get_alleles() const { return alleles_;         }
    const std::string& get_allele(int allele)     const { return alleles_[allele]; }
    const std::vector<std::string>& get_samples() const;
    int num_alleles() const { return alleles_.size(); }
    int num_gangstr_alleles() const;
    int num_samples() const { return num_samples_;    }
    int num_missing() const { return num_missing_;    }
    int num_alleles_by_length(const bool& round_alleles) const;
    int round_allele_length(int allele) const;
    float heterozygosity() const;
    float heterozygosity_by_length(const bool& round_alleles) const;
    
    // Build mapping of original GT index to length based index
    void build_alleles_by_length(const bool& round);
    // Convert between standard alleles in VCF to alleles ordered by length
    int GetLengthIndexFromGT(const int& gt_index) const;
    // Get length of allele from length-based allele index (length in bp relative to reference allele)
    int GetSizeFromLengthAllele(const int& length_gt_index) const;

    void destroy_record() {bcf_destroy(vcf_record_);}
    bool is_biallelic_snp() const {
      if (vcf_record_ != NULL)
	return (vcf_record_->n_allele == 2) && bcf_is_snp(vcf_record_);
      return false;
    }

    std::string get_chromosome() const {
      if (vcf_record_ != NULL)
	return bcf_seqname(vcf_header_, vcf_record_);
      else
	return "";
    }

    int32_t get_position() const {
      if (vcf_record_ != NULL)
	return vcf_record_->pos+1;
      else
	return -1;
    }

    std::string get_id() const {
      if (vcf_record_ != NULL)
	return vcf_record_->d.id;
      else
	return "";
    }

    bool has_format_field(const std::string& fieldname) const {
      return (bcf_get_fmt(vcf_header_, vcf_record_, fieldname.c_str()) != NULL);
    }

    bool has_info_field(const std::string& fieldname) const {
      return (bcf_get_info(vcf_header_, vcf_record_, fieldname.c_str()) != NULL);
    }

    bool sample_call_phased(int sample_index) const {
      return phased_[sample_index];
    }

    bool sample_call_missing(int sample_index) const {
      return missing_[sample_index];
    }

    bool sample_call_missing(const std::string& sample) const;

    void get_INFO_value_single_int(const std::string& fieldname, int32_t& val) const {
      int mem            = 0;
      int32_t* info_vals = NULL;
      if (bcf_get_info_int32(vcf_header_, vcf_record_, fieldname.c_str(), &info_vals, &mem) != 1)
	PrintMessageDieOnError("Failed to extract single INFO value from the VCF record", M_ERROR);
      val = info_vals[0];
      free(info_vals);
    }

    void get_INFO_value_multiple_ints(const std::string& fieldname, std::vector<int32_t>& vals) const {
      vals.clear();
      int mem             = 0;
      int32_t* info_vals  = NULL;
      int num_entries = bcf_get_info_int32(vcf_header_, vcf_record_, fieldname.c_str(), &info_vals, &mem);
      if (num_entries <= 1)
	PrintMessageDieOnError("Failed to extract multiple INFO values from the VCF record", M_ERROR);
      for (int i = 0; i < num_entries; i++)
	vals.push_back(info_vals[i]);
      free(info_vals);
    }

    void get_FORMAT_value_single_int(const std::string& fieldname, std::vector<int32_t>& vals) const {
      vals.clear();
      int mem = 0;
      int32_t* format_vals = NULL;
      int num_entries = bcf_get_format_int32(vcf_header_, vcf_record_, fieldname.c_str(), &format_vals, &mem);
      if (num_entries < num_samples()) {
	PrintMessageDieOnError("Failed to extract int FORMAT value from VCF record", M_ERROR);
      }
      int32_t* ptr = format_vals;
      for (int i = 0; i < num_samples(); i++) {
	vals.push_back(*ptr);
	ptr += 1;
      }
      free(format_vals);
    }

    void get_FORMAT_value_single_string(const std::string& fieldname, std::vector<std::string>& vals) const {
      vals.clear();
      int mem = 0;
      char** format_vals = NULL;
      int num_entries = bcf_get_format_string(vcf_header_, vcf_record_, fieldname.c_str(), &format_vals, &mem);
      if (num_entries < num_samples()) {
	PrintMessageDieOnError("Failed to extract string FORMAT value from VCF record", M_ERROR);
      }
      char** ptr = format_vals;
      for (int i = 0; i < num_samples(); i++) {
	vals.push_back(*ptr);
	ptr += 1;
      }
      free(format_vals[0]);
      free(format_vals);
    }

    void get_FORMAT_value_single_float(const std::string& fieldname, std::vector<float>& vals) const {
      vals.clear();
      int mem = 0;
      float* format_vals = NULL;
      int num_entries = bcf_get_format_float(vcf_header_, vcf_record_, fieldname.c_str(), &format_vals, &mem);
      if (num_entries < num_samples()) {
	PrintMessageDieOnError("Failed to extract int FORMAT value from VCF record", M_ERROR);
      }
      float* ptr = format_vals;
      for (int i = 0; i < num_samples(); i++) {
	vals.push_back(*ptr);
	ptr += 1;
      }
      free(format_vals);
    }

    void get_FORMAT_value_multiple_ints(const std::string& fieldname, std::vector<std::vector<int32_t> > & vals) const {
      vals.clear();
      int mem            = 0;
      int32_t* format_vals = NULL;
      int num_entries    = bcf_get_format_float(vcf_header_, vcf_record_, fieldname.c_str(), &format_vals, &mem);
      if (num_entries <= num_samples())
	PrintMessageDieOnError("Failed to extract multiple FORMAT values from the VCF record", M_ERROR);
      int entries_per_sample = num_entries/num_samples();
      int32_t* ptr = format_vals;
      for (int i = 0; i < num_samples(); i++){
	vals.push_back(std::vector<int32_t>(ptr, ptr+entries_per_sample));
	ptr += entries_per_sample;
      }
      free(format_vals);
    }

    void get_FORMAT_value_multiple_floats(const std::string& fieldname, std::vector< std::vector<float> >& vals) const {
      vals.clear();
      int mem            = 0;
      float* format_vals = NULL;
      int num_entries    = bcf_get_format_float(vcf_header_, vcf_record_, fieldname.c_str(), &format_vals, &mem);
      if (num_entries <= num_samples())
	PrintMessageDieOnError("Failed to extract multiple FORMAT values from the VCF record", M_ERROR);
      int entries_per_sample = num_entries/num_samples();
      float* ptr = format_vals;
      for (int i = 0; i < num_samples(); i++){
	vals.push_back(std::vector<float>(ptr, ptr+entries_per_sample));
	ptr += entries_per_sample;
      }
      free(format_vals);
    }

    void get_length_genotype(const std::string& sample, int& gt_al, int& gt_bl) const;
    void get_length_genotype(int sample_index, int& gt_al, int& gt_bl) const;

    void get_genotype(const std::string& sample, int& gt_a, int& gt_b) const;

    void get_genotype(int sample_index, int& gt_a, int& gt_b) const{
      gt_a = gt_1_[sample_index];
      gt_b = gt_2_[sample_index];
    }
  };

  class VCFReader {
  private:
    htsFile*   vcf_input_;
    kstring_t  vcf_line_;
    bcf1_t*    vcf_record_;
    tbx_t*     tbx_input_;
    hts_itr_t* tbx_iter_;
    bcf_hdr_t* vcf_header_;
    bool       jumped_;
    int        chrom_index_;
    std::vector<std::string> samples_;
    std::vector<std::string> chroms_;
    std::map<std::string, int> sample_indices_;

    void open(const std::string& filename);

    // Private unimplemented copy constructor and assignment operator to prevent operations
    VCFReader(const VCFReader& other);
    VCFReader& operator=(const VCFReader& other);

  public:
    explicit VCFReader(const std::string& filename){
      vcf_input_  = NULL;
      tbx_input_  = NULL;
      tbx_iter_   = NULL;
      jumped_     = false;
      vcf_line_.l = 0;
      vcf_line_.m = 0;
      vcf_line_.s = NULL;
      vcf_record_ = NULL;
      //      vcf_record_ = bcf_init();
      open(filename);
    }

    ~VCFReader(){
      if (vcf_input_  != NULL)   hts_close(vcf_input_);
      if (vcf_header_ != NULL)   bcf_hdr_destroy(vcf_header_);
      if (tbx_iter_   != NULL)   tbx_itr_destroy(tbx_iter_);
      if (tbx_input_  != NULL)   tbx_destroy(tbx_input_);
      if (vcf_line_.s != NULL)   free(vcf_line_.s);
      //bcf_destroy(vcf_record_);
    }

    bool has_sample(const std::string& sample) const {
      return sample_indices_.find(sample) != sample_indices_.end();
    }

    bool has_chromosome(const std::string& chrom) const {
      return tbx_name2id(tbx_input_, chrom.c_str()) != -1;
    }

    int get_sample_index(const std::string& sample) const {
      auto sample_iter = sample_indices_.find(sample);
      if (sample_iter == sample_indices_.end())
	return -1;
      else
	return sample_iter->second;
    }
  
    bool set_region(const std::string& region){
      tbx_itr_destroy(tbx_iter_);
      tbx_iter_ = tbx_itr_querys(tbx_input_, region.c_str());
      jumped_  = true;
      return tbx_iter_ != NULL;
    }

    bool set_region(const std::string& chrom, int32_t start, int32_t end = 0){
      std::stringstream region_str;
      if (end) region_str << chrom << ":" << start << "-" << end;
      else     region_str << chrom << ":" << start;
      return set_region(region_str.str());
    }

    const std::vector<std::string>& get_samples() const { return samples_; }

    bool get_next_variant(Variant* variant, const bool& round_allele);
  };

}
#endif
