#include "cms_runII_data_proc/processing/interface/file_looper.hh"

FileLooper::FileLooper(bool return_all, std::vector<std::string> requested, bool use_deep_bjet_wps,
                       bool apply_cut, bool inc_all_jets, bool inc_other_regions, bool inc_data, bool inc_unc, bool only_kl1) {
    _evt_proc = new EvtProc(return_all, requested, use_deep_bjet_wps);
    _feat_names = _evt_proc->get_feats();
    _n_feats = _feat_names.size();
    _apply_cut = apply_cut;
    _inc_all_jets = inc_all_jets;
    _inc_other_regions = inc_other_regions;
    _inc_unc = inc_unc;
    _inc_data = inc_data;
    _only_kl1 = only_kl1;
}

FileLooper::~FileLooper() {
    delete _evt_proc;
}

bool FileLooper::loop_file(const std::string& in_dir, const std::string& out_dir, const std::string& channel, const std::string& year,
                           const long int& n_events) {
    /*
    Loop though file {in_dir}/{year}_{channel}.root processing {n_events} (all events if n_events < 0).
    Processed events will be saved to one of two trees inside {out_dir}/{year}_{channel}.root:
    Even event IDs will be saved to data_0 and odd to data_1.
    */

    std::string fname = in_dir+"/"+year+"_"+channel+".root";
    std::cout << "Reading from file: " << fname << "\n";
    TFile* in_file = TFile::Open(fname.c_str());
    TTreeReader reader(channel.c_str(), in_file);

    // Enums
    Channel e_channel = FileLooper::_get_channel(channel);
    Year e_year = FileLooper::_get_year(year);
    Spin spin(nonres);
    float klambda;

    // Meta info
    std::cout << "Extracting auxiliary data...";
    TTreeReaderValue<std::vector<double>> rv_weight(reader, "all_weights");
    TTreeReaderValue<unsigned long long> rv_evt(reader, "evt");
    TTreeReaderValue<std::vector<unsigned long>> rv_id(reader, "dataIds");
    std::map<unsigned long, std::string> id2name = FileLooper::build_id_map(in_file);
    std::cout << " Extracted\n";
    double weight;
    float res_mass;
    std::vector<std::string> names;
    int sample, region, jet_cat, n_vbf, class_id;
    std::vector<unsigned int> idxs;
    unsigned long long int strat_key, evt;
    bool scale, central_unc, svfit_conv, hh_kinfit_conv, accept, cut_pass;
    std::vector<unsigned long> ids;

    // HL feats
    TTreeReaderValue<float> rv_kinfit_mass(reader, "m_ttbb_kinfit");
    TTreeReaderValue<float> rv_kinfit_chi2(reader, "chi2_kinFit");
    TTreeReaderValue<float> rv_mt2(reader, "MT2");
    TTreeReaderValue<float> rv_mt_tot(reader, "mt_tot");
    TTreeReaderValue<float> rv_top_1_mass(reader, "mass_top1");
    TTreeReaderValue<float> rv_top_2_mass(reader, "mass_top2");
    TTreeReaderValue<float> rv_p_zetavisible(reader, "p_zetavisible");
    TTreeReaderValue<float> rv_p_zeta(reader, "p_zeta");
    float kinfit_mass, kinfit_chi2, mt2, mt_tot, top_1_mass, top_2_mass, p_zetavisible, p_zeta;

    // Tagging
    TTreeReaderValue<float> rv_b_1_csv(reader, "b1_DeepFlavour");
    TTreeReaderValue<float> rv_b_2_csv(reader, "b2_DeepFlavour");
    float b_1_csv, b_2_csv;
    bool is_boosted;

    // SVFit feats
    TTreeReaderValue<float> rv_svfit_pT(reader, "pt_sv");
    TTreeReaderValue<float> rv_svfit_eta(reader, "eta_sv");
    TTreeReaderValue<float> rv_svfit_phi(reader, "phi_sv");
    TTreeReaderValue<float> rv_svfit_mass(reader, "m_sv");
    LorentzVectorPEP pep_svfit;
    LorentzVector svfit;

    // l1 feats
    TTreeReaderValue<float> rv_l_1_pT(reader, "pt_1");
    TTreeReaderValue<float> rv_l_1_eta(reader, "eta_1");
    TTreeReaderValue<float> rv_l_1_phi(reader, "phi_1");
    TTreeReaderValue<float> rv_l_1_mass(reader, "m_1");
    TTreeReaderValue<float> rv_l_1_mt(reader, "mt_1");
    float l_1_mass, l_1_mt;
    LorentzVectorPEP pep_l_1;
    LorentzVector l_1;


    // l2 feats
    TTreeReaderValue<float> rv_l_2_pT(reader, "pt_2");
    TTreeReaderValue<float> rv_l_2_eta(reader, "eta_2");
    TTreeReaderValue<float> rv_l_2_phi(reader, "phi_2");
    TTreeReaderValue<float> rv_l_2_mass(reader, "m_2");
    TTreeReaderValue<float> rv_l_2_mt(reader, "mt_2");
    float l_2_mt;
    LorentzVectorPEP pep_l_2;\
    LorentzVector l_2;

    // MET feats
    TTreeReaderValue<float> rv_met_pT(reader, "pt_MET");
    TTreeReaderValue<float> rv_met_phi(reader, "phiMET");
    LorentzVectorPEP pep_met;
    LorentzVector met;

    // b1 feats
    TTreeReaderValue<float> rv_b_1_pT(reader, "pt_b1");
    TTreeReaderValue<float> rv_b_1_eta(reader, "eta_b1");
    TTreeReaderValue<float> rv_b_1_phi(reader, "phi_b1");
    TTreeReaderValue<float> rv_b_1_mass(reader, "m_b1");
    LorentzVectorPEP pep_b_1;
    LorentzVector b_1;

    // b2 feats
    TTreeReaderValue<float> rv_b_2_pT(reader, "pt_b2");
    TTreeReaderValue<float> rv_b_2_eta(reader, "eta_b2");
    TTreeReaderValue<float> rv_b_2_phi(reader, "phi_b2");
    TTreeReaderValue<float> rv_b_2_mass(reader, "m_b2");
    LorentzVectorPEP pep_b_2;
    LorentzVector b_2;

    // vbf1 feats
    TTreeReaderValue<float> rv_vbf_1_pT(reader, "pt_VBF_1");
    TTreeReaderValue<float> rv_vbf_1_eta(reader, "eta_VBF_1");
    TTreeReaderValue<float> rv_vbf_1_phi(reader, "phi_VBF_1");
    TTreeReaderValue<float> rv_vbf_1_mass(reader, "m_VBF_1");
    LorentzVectorPEP pep_vbf_1;
    LorentzVector vbf_1;

    // vbf2 feats
    TTreeReaderValue<float> rv_vbf_2_pT(reader, "pt_VBF_2");
    TTreeReaderValue<float> rv_vbf_2_eta(reader, "eta_VBF_2");
    TTreeReaderValue<float> rv_vbf_2_phi(reader, "phi_VBF_2");
    TTreeReaderValue<float> rv_vbf_2_mass(reader, "m_VBF_2");
    LorentzVectorPEP pep_vbf_2;
    LorentzVector vbf_2;

    std::vector<std::unique_ptr<float>> feat_vals;
    feat_vals.reserve(_n_feats);
    for (unsigned int i = 0; i < _n_feats; i++) feat_vals.emplace_back(new float(0));
    
    // Outfiles
    std::string oname = out_dir+"/"+year+"_"+channel+".root";
    std::cout << "Preparing output file: " << oname << " ...";
    TFile* out_file  = new TFile(oname.c_str(), "recreate");
    TTree* data_even = new TTree("data_0", "Even id data");
    TTree* data_odd  = new TTree("data_1", "Odd id data");
    FileLooper::_prep_file(data_even, feat_vals, &weight, &sample, &region, &jet_cat, &cut_pass, &scale, &central_unc, &class_id, &strat_key);
    FileLooper::_prep_file(data_odd,  feat_vals, &weight, &sample, &region, &jet_cat, &cut_pass, &scale, &central_unc, &class_id, &strat_key);
    std::cout << "\tprepared.\nBeginning loop.\n";

    long int c_event(0), n_tot_events(reader.GetEntries(true));
    while (reader.Next()) {
        c_event++;
        if (c_event%1000 == 0) std::cout << c_event << " / " << n_tot_events << "\n";
        ids = *rv_id;

        names = FileLooper::_get_evt_names(id2name, ids);
        if (names.size() == 0) continue;
        FileLooper::_extract_flags(names, sample, region, central_unc, scale, jet_cat, cut_pass, class_id, spin, klambda, res_mass, is_boosted, accept, idxs);
        if (!accept) continue;
        strat_key = FileLooper::_get_strat_key(sample, jet_cat, region, static_cast<int>(central_unc), static_cast<int>(cut_pass));

        // Load meta
        weight = FileLooper::_get_weight(rv_weight, idxs); 
        evt    =  *rv_evt;

        // Load HL feats
        kinfit_mass   = *rv_kinfit_mass;
        kinfit_chi2   = *rv_kinfit_chi2;
        mt2           = *rv_mt2;
        mt_tot        = *rv_mt_tot;
        top_1_mass    = *rv_top_1_mass;
        top_2_mass    = *rv_top_2_mass;
        p_zetavisible = *rv_p_zetavisible;
        p_zeta        = *rv_p_zeta;
        l_1_mt        = *rv_l_1_mt;
        l_2_mt        = *rv_l_2_mt;

        // Load tagging
        b_1_csv     = *rv_b_1_csv;
        b_2_csv     = *rv_b_2_csv;

        // Load vectors
        pep_svfit.SetCoordinates(*rv_svfit_pT, *rv_svfit_eta, *rv_svfit_phi, *rv_svfit_mass);
        if (channel == "muTau") {  // Fix mass for light leptons
            l_1_mass = MU_MASS;
        } else if (channel == "eTau") {
            l_1_mass = E_MASS;
        } else {
            l_1_mass = *rv_l_1_mass;
        }
        pep_l_1.SetCoordinates(*rv_l_1_pT, *rv_l_1_eta, *rv_l_1_phi, l_1_mass);
        pep_l_2.SetCoordinates(*rv_l_2_pT, *rv_l_2_eta, *rv_l_2_phi, *rv_l_2_mass);
        pep_met.SetCoordinates(*rv_met_pT, 0,           *rv_met_phi, 0);
        pep_b_1.SetCoordinates(*rv_b_1_pT, *rv_b_1_eta, *rv_b_1_phi, *rv_b_1_mass);
        pep_b_2.SetCoordinates(*rv_b_2_pT, *rv_b_2_eta, *rv_b_2_phi, *rv_b_2_mass);
        pep_vbf_1.SetCoordinates(*rv_vbf_1_pT, *rv_vbf_1_eta, *rv_vbf_1_phi, *rv_vbf_1_mass);
        pep_vbf_2.SetCoordinates(*rv_vbf_2_pT, *rv_vbf_2_eta, *rv_vbf_2_phi, *rv_vbf_2_mass);

        svfit.SetCoordinates(pep_svfit.Px(), pep_svfit.Py(), pep_svfit.Pz(), pep_svfit.M());
        l_1.SetCoordinates(pep_l_1.Px(),     pep_l_1.Py(),   pep_l_1.Pz(),   pep_l_1.M());
        l_2.SetCoordinates(pep_l_2.Px(),     pep_l_2.Py(),   pep_l_2.Pz(),   pep_l_2.M());
        met.SetCoordinates(pep_met.Px(),     pep_met.Py(),   0,              0);
        b_1.SetCoordinates(pep_b_1.Px(),     pep_b_1.Py(),   pep_b_1.Pz(),   pep_b_1.M());
        b_2.SetCoordinates(pep_b_2.Px(),     pep_b_2.Py(),   pep_b_2.Pz(),   pep_b_2.M());
        vbf_1.SetCoordinates(pep_vbf_1.Px(), pep_vbf_1.Py(), pep_vbf_1.Pz(), pep_vbf_1.M());
        vbf_2.SetCoordinates(pep_vbf_2.Px(), pep_vbf_2.Py(), pep_vbf_2.Pz(), pep_vbf_2.M());

        // VBF
        n_vbf = 0;
        if (jet_cat == 4) {
            if (*rv_vbf_1_mass != std::numeric_limits<float>::lowest()) n_vbf++;
            if (*rv_vbf_2_mass != std::numeric_limits<float>::lowest()) n_vbf++;
        }

        // Convergence
        svfit_conv     = *rv_svfit_mass > 0;
        hh_kinfit_conv = kinfit_chi2    > 0;

        _evt_proc->process_to_vec(feat_vals, b_1, b_2, l_1, l_2, met, svfit, vbf_1, vbf_2, kinfit_mass, kinfit_chi2, mt2, mt_tot, p_zetavisible, p_zeta, top_1_mass,
                                  top_2_mass, l_1_mt, l_2_mt, is_boosted, b_1_csv, b_2_csv, e_channel, e_year, res_mass, spin,
                                  klambda, n_vbf, svfit_conv, hh_kinfit_conv);

        if (evt%2 == 0) {
            data_even->Fill();
        } else {
            data_odd->Fill();
        }        
        if (n_events > 0 && c_event >= n_events) {
            std::cout << "Exiting after " << c_event << " events.\n";
            break;
        }
    }

    std::cout << "Loop complete, saving results.\n";
    data_even->Write();
    data_odd->Write();
    delete data_even;
    delete data_odd;
    in_file->Close();
    out_file->Close();
    return true;
}

void FileLooper::_prep_file(TTree* tree, const std::vector<std::unique_ptr<float>>& feat_vals, double* weight, int* sample, int* region, int* jet_cat,
                            bool* cut_pass, bool* scale, bool* central_unc, int* class_id, unsigned long long int* strat_key) {
    /* Add branches to tree and set addresses for values */

    for (unsigned int i = 0; i < _n_feats; i++) tree->Branch(_feat_names[i].c_str(), feat_vals[i].get());
    tree->Branch("weight",      weight);
    tree->Branch("sample",      sample);
    tree->Branch("region",      region);
    tree->Branch("jet_cat",     jet_cat);
    tree->Branch("cut_pass",    cut_pass);
    tree->Branch("scale",       scale);
    tree->Branch("central_unc", central_unc);
    tree->Branch("class_id",    class_id);
    tree->Branch("strat_key",   strat_key);
}

Channel FileLooper::_get_channel(std::string channel) {
    /* COnvert channel to enum */

    if (channel == "tauTau") {
        return Channel(tauTau);
    } else if (channel == "muTau") {
        return Channel(muTau);
    } else if (channel == "eTau") {
        return Channel(eTau);
    }
    throw std::invalid_argument("Invalid channel: options are tauTau, muTau, eTau");
    return Channel(tauTau);
}

Year FileLooper::_get_year(std::string year) {
    /* Convert year to enum */

    if (year == "2016") {
        return Year(y16);
    } else if (year == "2017") {
        return Year(y17);
    } else if (year == "2018") {
        return Year(y18);
    }
    throw std::invalid_argument("Invalid year: options are 2016, 2017, 2018");
    return Year(y16);
}

std::vector<std::string> FileLooper::_get_evt_names(const std::map<unsigned long, std::string>& id2name, const std::vector<unsigned long>& ids) {
    /* Match data IDs to aux names */
    
    std::vector<std::string> names(ids.size());
    for (unsigned int i = 0; i < ids.size(); i++) names[i] = id2name.at(ids[i]);
    return names;
}

void FileLooper::_extract_flags(const std::vector<std::string>& names, int& sample, int& region, bool& central_unc, bool& scale,
                                int& jet_cat, bool& cut_pass, int& class_id, Spin& spin, float& klambda, float& res_mass,
                                bool& is_boosted, bool& accept, std::vector<unsigned int>& idxs) {
    /*
    Extract event flags from name 
    Example: "2j/NoCuts/SS_AntiIsolated/None/Central/DY_MC_M-10-50"
    */

    std::string val;
    bool tmp_cut_pass;
    std::vector<int> tmp_jet_cats;
    std::vector<std::string> tmp_cut_passes;
    int tmp_jet_cat;
    idxs.clear();
    float tmp_klambda;
    for (unsigned int n = 0; n < names.size(); n++) {
        std::istringstream iss(names[n]);
        int i = 0;
        while (std::getline(iss, val, '/')) {   
            if (i == 0) {
                tmp_jet_cat = FileLooper::_jet_cat_lookup(val);
            } else if (i == 1) {
                tmp_cut_pass = (val == "mh");
            } else if (i == 2 &&  n == 0) {
                region = FileLooper::_region_lookup(val);
            } else if (i == 3 &&  n == 0) {
                central_unc = (val == "None");
            } else if (i == 4 &&  n == 0) {
                scale = (val == "Central");
            } else if (i == 5 &&  n == 0) {
                FileLooper::_sample_lookup(val, sample, spin, tmp_klambda, res_mass);
                class_id = FileLooper::_sample2class_lookup(sample);
            }
            i++;
        }
        if (FileLooper::_accept_evt(region, central_unc, tmp_jet_cat, tmp_cut_pass, class_id, tmp_klambda)) {
            tmp_jet_cats.push_back(tmp_jet_cat);
            tmp_cut_passes.push_back(tmp_cut_pass);
            idxs.push_back(n);
        }
    }
    if (idxs.size() == 0) {
        accept = false;
    } else {
        accept = true;
        jet_cat = -1;
        for (unsigned int i = 0; i < idxs.size(); i++) {
            if (tmp_jet_cats[i] == 4) is_boosted = true;
            if (tmp_jet_cats[i] > jet_cat) jet_cat = tmp_jet_cats[i];
            if (tmp_cut_passess[i]) cut_pass = true;
        }
    }
}

int FileLooper::_jet_cat_lookup(const std::string& jet_cat) {
    if (jet_cat == "2j")            return 0;
    if (jet_cat == "2j0bR_noVBF")   return 1;
    if (jet_cat == "2j1bR_noVBF")   return 2;
    if (jet_cat == "2j2b+R_noVBF")  return 3;
    if (jet_cat == "2j2Lb+B_noVBF") return 4;
    if (jet_cat == "2j1b+_VBFL")    return 5;
    if (jet_cat == "2j1b+_VBF")     return 6;
    throw std::invalid_argument("Unrecognised jet category: " + jet_cat);
    return -1;
}

int FileLooper::_region_lookup(const std::string& region) {
    if (region == "OS_Isolated")      return 0;
    if (region == "OS_AntiIsolated")  return 1;
    if (region == "SS_Isolated")      return 2;
    if (region == "SS_AntiIsolated")  return 3;
    if (region == "SS_LooseIsolated") return 4;
    throw std::invalid_argument("Unrecognised region: " + region);
    return -1;
}

void FileLooper::_sample_lookup(const std::string& sample, int& sample_id, Spin& spin, float& klambda, float& res_mass) {
    spin = nonres;
    res_mass = 125;
    klambda = 1;
    
    if (sample.find("GluGluSignal") != std::string::npos) { 
        if (sample.find("NonRes") != std::string::npos) {
            sample_id = -12;
            try {
                klambda = std::stof(sample.substr(sample.find("_kl")+3));
            } catch (...) {
                std::cout << "Error in sample " << sample << " attempting to parse " << sample.substr(sample.find("_kl")+3) << "\n";
                assert(false);
            }
        } else if (sample.find("Radion") != std::string::npos) {
            spin = radion;
            try {
                res_mass = std::stof(sample.substr(sample.find("_M")+2));
            } catch (...) {
                std::cout << "Error in sample " << sample << " attempting to parse " << sample.substr(sample.find("_M")+2) << "\n";
                assert(false);
            }
            if (res_mass <= 400) {
                sample_id = -13;
            } else if (res_mass <= 600) {
                sample_id = -14;
            } else {
                sample_id = -15;
            }
        } else if (sample.find("Graviton") != std::string::npos) {
            spin = graviton;
            try {
                res_mass = std::stof(sample.substr(sample.find("_M")+2));
            } catch (...) {
                std::cout << "Error in sample " << sample << " attempting to parse " << sample.substr(sample.find("_M")+2) << "\n";
                assert(false);
            }
            if (res_mass <= 400) {
                sample_id = -16;
            } else if (res_mass <= 600) {
                sample_id = -17;
            } else {
                sample_id = -18;
            }
        }
    } else if (sample.find("VBFSignal") != std::string::npos) {
        if (sample.find("NonRes") != std::string::npos) {
            sample_id = -19;
        } else if (sample.find("Radion") != std::string::npos) {
            spin = radion;
            try {
                res_mass = std::stof(sample.substr(sample.find("_M")+2));
            } catch (...) {
                std::cout << "Error in sample " << sample << " attempting to parse " << sample.substr(sample.find("_M")+2) << "\n";
                assert(false);
            }
            if (res_mass <= 400) {
                sample_id = -20;
            } else if (res_mass <= 600) {
                sample_id = -21;
            } else {
                sample_id = -22;
            }
        } else if (sample.find("Graviton") != std::string::npos) {
            spin = graviton;
            try {
                res_mass = std::stof(sample.substr(sample.find("_M")+2));
            } catch (...) {
                std::cout << "Error in sample " << sample << " attempting to parse " << sample.substr(sample.find("_M")+2) << "\n";
                assert(false);
            }
            if (res_mass <= 400) {
                sample_id = -23;
            } else if (res_mass <= 600) {
                sample_id = -24;
            } else {
                sample_id = -25;
            }
        }
    } else if (sample.find("Data") != std::string::npos) {
        sample_id = 0;
    } else if (sample.find("TT") != std::string::npos) {
        sample_id = 1;
    } else if (sample.find("ttH") != std::string::npos) {
        sample_id = 2;
    } else if (sample.find("DY") != std::string::npos) {
        sample_id = 3;
    } else if (sample.find("Wjets") != std::string::npos) {
        sample_id = 4;
    } else if (sample.find("SM_Higgs") != std::string::npos) {
        sample_id = 5;
    } else if (sample.find("VH") != std::string::npos) {
        sample_id = 6;
    } else if (sample.find("VVV") != std::string::npos) {
        sample_id = 7;
    } else if (sample.find("EWK") != std::string::npos) {
        sample_id = 8;
    } else if (sample.find("VV") != std::string::npos) {
        sample_id = 9;
    } else if (sample.find("ST") != std::string::npos) {
        sample_id = 10;
    } else if (sample.find("ttV") != std::string::npos) {
        sample_id = 11;
    } else{
        throw std::invalid_argument("Unrecognised sample: " + sample);
    }
}

int FileLooper::_sample2class_lookup(const int& sample) {
    if (sample < 0)  return 1;   // Signal
    if (sample == 0) return -1;  // Collider data
    return 0;                    // Background
}

bool FileLooper::_accept_evt(const int& region, const bool& central_unc, const int& jet_cat, const bool& cut_pass, const int& class_id, const float& klambda) {
    if (_only_kl1 && klambda != 1) return false; // Only consider klambda at SM point
    if (_apply_cut && !cut_pass) return false;  // Require mass cut and cut failed
    if (!_inc_data && class_id == -1) return false; // Don't include data and event is data
    if (!_inc_other_regions && region != 0) return false;  //Don't include other regions and event is not SS Iso
    if (!_inc_unc && !central_unc) return false;  //Don't systematics and event is a systematic
    if (!_inc_all_jets && jet_cat == 0) return false;  // Only use inference category jets and event is non-inference category
    return true;
}

unsigned long long int FileLooper::_get_strat_key(const int& sample, const int& jet_cat, const int& region, const int& central_unc,
                                                  const int& cut_pass) {
    unsigned long long int strat_key = std::pow(2,  std::abs(sample))*
                                       std::pow(3,  jet_cat)*
                                       std::pow(5, region)*
                                       std::pow(7, cut_pass)*
                                       std::pow(11, central_unc);
    if (strat_key == 0) throw std::overflow_error("Strat key overflow\n");    
    return strat_key;
}

std::map<unsigned long, std::string> FileLooper::build_id_map(TFile* in_file) {
    TTreeReader aux_reader("aux", in_file);
    TTreeReaderValue<std::vector<unsigned long>> rv_aux_id(aux_reader, "dataIds");
    TTreeReaderValue<std::vector<std::string>> rv_aux_name(aux_reader, "dataId_names");
    std::vector<unsigned long> ids;
    std::vector<std::string> names;
    while (aux_reader.Next()) {
        ids   = *rv_aux_id;
        names = *rv_aux_name;
    }
    std::map<unsigned long, std::string> id2name;
    for (unsigned int i = 0; i < ids.size(); i++) id2name[ids[i]] = names[i];
    return id2name;
}

double FileLooper::_get_weight(const TTreeReaderValue<std::vector<double>>& rv_weight, const std::vector<unsigned int>& idxs) {
    double del, weight = (*rv_weight)[idxs[0]];
    for (unsigned int i = 1; i < idxs.size(); i++) {
        del = std::abs((*rv_weight)[i]-weight);
        if (del < 1e-7) {
            std::cout << "Multiple weights found. Del = " << del << "\n";
            assert(false);
        }
    }
    return weight;
}