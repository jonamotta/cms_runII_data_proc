#include "cms_runII_data_proc/processing/interface/file_looper.hh"

FileLooper::FileLooper(bool return_all, std::set<std::string> requested, bool use_deep_csv,
                       bool apply_cut, bool inc_all_jets, bool inc_other_regions, bool inc_data, bool inc_unc) {
    _evt_proc = new EvtProc(return_all, requested, use_deep_csv);
    _feat_names = _evt_proc->get_feats();
    _n_feats = _feat_names.size();
    _apply_cut = apply_cut;
    _inc_all_jets = inc_all_jets;
    _inc_other_regions = inc_other_regions;
    _inc_unc = inc_unc;
    _inc_data = inc_data;
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

    TFile* in_file = TFile::Open((in_dir+"/"+year+"_"+channel+".root").c_str());
    TTreeReader reader(channel.c_str(), in_file);
    TTreeReader aux_reader("aux", in_file);

    // Enums
    Channel e_channel = FileLooper::_get_channel(channel);
    Year e_year = FileLooper::_get_year(year);
    Spin spin(nonres);
    float klambda = 1;

    // Meta info
    TTreeReaderValue<double> rv_weight(reader, "weight");  // TODO: Check this
    TTreeReaderValue<std::vector<unsigned long>> rv_id(reader, "dataIds");
    TTreeReaderValue<std::vector<unsigned long>> rv_aux_id(aux_reader, "dataIds");
    TTreeReaderValue<std::vector<std::string>> rv_aux_name(aux_reader, "dataId_names");
    double weight;
    float res_mass = 0;
    std::string name;
    int sample, region, jet_cat;
    unsigned long long int strat_key;
    bool cut, scale, syst_unc;
    unsigned long int id;
    int class_id;

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
    TTreeReaderValue<float> rv_b_1_csv(reader, "csv_b1");
    TTreeReaderValue<float> rv_b_2_csv(reader, "csv_b2");
    TTreeReaderValue<float> rv_b_1_deepcsv(reader, "deepcsv_b1");
    TTreeReaderValue<float> rv_b_2_deepcsv(reader, "deepcsv_b2");
    float b_1_csv, b_2_csv, b_1_deepcsv, b_2_deepcsv;
    bool is_boosted;

    // SVFit feats
    //TTreeReaderValue<float> rv_svfit_pT(reader, "pt_sv");
    //TTreeReaderValue<float> rv_svfit_eta(reader, "eta_sv");
    //TTreeReaderValue<float> rv_svfit_phi(reader, "phi_sv");
    TTreeReaderValue<float> rv_svfit_mass(reader, "m_sv");
    LorentzVectorPEP pep_svfit;
    LorentzVector svfit;

    // l1 feats
    TTreeReaderValue<float> rv_l_1_pT(reader, "pt_1");
    TTreeReaderValue<float> rv_l_1_eta(reader, "eta_1");
    TTreeReaderValue<float> rv_l_1_phi(reader, "phi_1");
    TTreeReaderValue<float> rv_l_1_mass(reader, "m_1");
    TTreeReaderValue<float> rv_l_1_mt(reader, "mt_1");
    float l_1_mt;
    LorentzVectorPEP pep_l_1;
    LorentzVector l_1;


    // l2 feats
    TTreeReaderValue<float> rv_l_2_pT(reader, "pt_2");
    TTreeReaderValue<float> rv_l_2_eta(reader, "eta_2");
    TTreeReaderValue<float> rv_l_2_phi(reader, "phi_2");
    TTreeReaderValue<float> rv_l_2_mass(reader, "m_2");
    TTreeReaderValue<float> rv_l_2_mt(reader, "mt_2");
    float l_2_mass, l_2_mt;
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

    std::vector<float*> feat_vals(_n_feats);
    
    // Outfiles
    TFile* out_file  = new TFile((out_dir+"/"+year+"_"+channel+".root").c_str(), "recreate");
    TTree* data_even = new TTree("data_0", "Even id data");
    TTree* data_odd  = new TTree("data_1", "Odd id data");
    FileLooper::_prep_file(data_even, feat_vals, weight, sample, region, jet_cat, cut, scale, syst_unc, class_id, strat_key);
    FileLooper::_prep_file(data_odd,  feat_vals, weight, sample, region, jet_cat, cut, scale, syst_unc, class_id, strat_key);

    long int c_event(0), n_tot_events(reader.GetEntries(true));
    while (reader.Next()) {
        if (c_event%1000 == 0) std::cout << c_event << " / " << n_tot_events;
        id = (*rv_id)[0];

        std::cout << id << " : [" ;
        for (auto i : *rv_id) std::cout << i << " ";
        std::cout << "]\n";
        break;

        name = FileLooper::_get_evt_name(aux_reader, rv_aux_id, rv_aux_name, id);
        FileLooper::_extract_flags(name, sample, region, syst_unc, scale, jet_cat, cut, class_id, spin, klambda, res_mass, is_boosted);
        if (!FileLooper::_accept_evt(region, syst_unc, jet_cat, cut, class_id)) continue;
        strat_key = FileLooper::_get_strat_key(sample, static_cast<int>(klambda), static_cast<int>(res_mass), static_cast<int>(jet_cat), region,
                                               static_cast<int>(spin), static_cast<int>(syst_unc), static_cast<int>(cut));

        // Load meta
        weight = *rv_weight;

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
        b_1_deepcsv = *rv_b_1_deepcsv;
        b_2_deepcsv = *rv_b_2_deepcsv;

        // Load vectors
        // pep_svfit.SetCoordinates(*rv_svfit_pT, *rv_svfit_eta, *rv_svfit_phi, *rv_svfit_mass);
        pep_svfit.SetCoordinates(0, 0, 0, *rv_svfit_mass); // TODO remove once svfit present
        pep_l_1.SetCoordinates(*rv_l_1_pT, *rv_l_1_eta, *rv_l_1_phi, *rv_l_1_mass);
        if (channel == "muTau") {  // Fix mass for light leptons
            l_2_mass = MU_MASS;
        } else if (channel == "eTau") {
            l_2_mass = E_MASS;
        } else {
            l_2_mass = *rv_l_2_mass;
        }
        pep_l_2.SetCoordinates(*rv_l_2_pT, *rv_l_2_eta, *rv_l_2_phi, l_2_mass);
        pep_met.SetCoordinates(*rv_met_pT, 0,           *rv_met_phi, 0);
        pep_b_1.SetCoordinates(*rv_b_1_pT, *rv_b_1_eta, *rv_b_1_phi, *rv_b_1_mass);
        pep_b_2.SetCoordinates(*rv_b_2_pT, *rv_b_2_eta, *rv_b_2_phi, *rv_b_2_mass);

        svfit.SetCoordinates(pep_svfit.Px(), pep_svfit.Py(), pep_svfit.Pz(), pep_svfit.M());
        l_1.SetCoordinates(pep_l_1.Px(), pep_l_1.Py(), pep_l_1.Pz(), pep_l_1.M());
        l_2.SetCoordinates(pep_l_2.Px(), pep_l_2.Py(), pep_l_2.Pz(), pep_l_2.M());
        met.SetCoordinates(pep_met.Px(), pep_met.Py(), 0,            0);
        b_1.SetCoordinates(pep_b_1.Px(), pep_b_1.Py(), pep_b_1.Pz(), pep_b_1.M());
        b_2.SetCoordinates(pep_b_2.Px(), pep_b_2.Py(), pep_b_2.Pz(), pep_b_2.M());

        _evt_proc->process_to_vec(feat_vals, b_1, b_2, l_1, l_2, met, svfit, kinfit_mass, kinfit_chi2, mt2, mt_tot, p_zetavisible, p_zeta, top_1_mass,
                                  top_2_mass, l_1_mt, l_2_mt, is_boosted, b_1_csv, b_2_csv, b_1_deepcsv, b_2_deepcsv, e_channel, e_year, res_mass, spin,
                                  klambda);

        
        if (c_event%2 == 0) {  // TODO: Replace with evt once implemented
            data_even->Fill();
        } else {
            data_odd->Fill();
        }
        
        c_event++;
        if (c_event >= n_events) break;
    }

    delete data_even;
    delete data_odd;
    in_file->Close();
    out_file->Close();
    return true;
}

void FileLooper::_prep_file(TTree* tree, const std::vector<float*>& feat_vals, const double& weight, const int& sample, const int& region, const int& jet_cat,
                            const bool& cut, const bool& scale, const bool& syst_unc, const int& class_id, const unsigned long long int& strat_key) {
    /* Add branches to tree and set addresses for values */

    for (unsigned int i = 0; i < _n_feats; i++) tree->Branch(_feat_names[i].c_str(), feat_vals[i]);
    tree->Branch("weight",    weight);
    tree->Branch("sample",    sample);
    tree->Branch("region",    region);
    tree->Branch("jet_cat",   jet_cat);
    tree->Branch("cut",       cut);
    tree->Branch("scale",     scale);
    tree->Branch("syst_unc",  syst_unc);
    tree->Branch("class_id",  class_id);
    tree->Branch("strat_key", strat_key);
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

std::string FileLooper::_get_evt_name(TTreeReader& aux_reader, TTreeReaderValue<std::vector<unsigned long>>& rv_aux_id,
                                      TTreeReaderValue<std::vector<std::string>>& rv_aux_name, const unsigned long int& id) {
    /* Match data ID to aux name */
    
    aux_reader.Restart();
    std::string name;
    while (aux_reader.Next()) {
        std::cout << id << " : [" ;
        for (auto i : *rv_aux_id) std::cout << i << " ";
        std::cout << "]\n";
        if ((*rv_aux_id)[0] == id) {
            name = (*rv_aux_name)[0];
            break;
        }
    }
    if (name == "") throw std::runtime_error("ID not found\n");
    return name;
}

void FileLooper::_extract_flags(const std::string& name, int& sample, int& region, bool& syst_unc, bool& scale, int& jet_cat, bool& cut, int& class_id,
                                Spin& spin, float& klambda, float& res_mass, bool& is_boosted) {
    /*
    Extract event flags from name 
    Example: "2j/NoCuts/SS_AntiIsolated/None/Central/DY_MC_M-10-50"
    */

    std::string val;
    std::istringstream iss(name);
    int i = 0;
    while (std::getline(iss, val, '/')) {   
        if (i == 0) {
            jet_cat = FileLooper::_jet_cat_lookup(val);
            is_boosted = false;  // TODO: update this
        } else if (i == 1) {
            cut = (val == "NoCuts");
        } else if (i == 2) {
            region = FileLooper::_region_lookup(val);
        } else if (i == 3) {
            syst_unc = (val == "None");
        } else if (i == 4) {
            scale = (val == "Central");
        } else if (i == 5) {
            FileLooper::_sample_lookup(val, sample, spin, klambda, res_mass);
        }
        i++;
    }
    class_id = FileLooper::_sample2class_lookup(sample);
}

int FileLooper::_jet_cat_lookup(const std::string& jet_cat) {
    if (jet_cat == "2j")           return 0;
    if (jet_cat == "2j0bR_noVBF")  return 1;
    if (jet_cat == "2j1bR_noVBF")  return 2;
    if (jet_cat == "2j2b+R_noVBF") return 3;
    if (jet_cat == "4j1b+_VBF")    return 4;
    // TODO: Boosted?
    throw std::invalid_argument("Unrecognised jet category: " + jet_cat);
    return -1;
}

int FileLooper::_region_lookup(const std::string& region) {
    if (region == "OS_Isolated")      return 0;
    if (region == "OS_AntiIsolated")  return 1;
    if (region == "SS_Isolated")      return 2;
    if (region == "SS_AntiIsolated")  return 3;
    throw std::invalid_argument("Unrecognised region: " + region);
    return -1;
}

void FileLooper::_sample_lookup(const std::string& sample, int& sample_id, Spin& spin, float& klambda, float& res_mass) {
    if (sample.find("/Signal_NonRes") != std::string::npos) {
        spin = nonres;
        res_mass = 125;
        sample_id = -125;
        klambda = std::stof(sample.substr(sample.find("_kl")+3));
    } else if (sample.find("/Signal_Radion") != std::string::npos) {
        spin = radion;
        res_mass = std::stof(sample.substr(sample.find("_M")+2));
        sample_id = -res_mass;
    } else if (sample.find("/Signal_Graviton") != std::string::npos) {
        spin = graviton;
        res_mass = std::stof(sample.substr(sample.find("_M")+2));
        sample_id = -res_mass;
    } else if (sample.find("/Data") != std::string::npos) {
        sample_id = 0;
    } else if (sample.find("/TT") != std::string::npos) {
        sample_id = 1;
    } else if (sample.find("/ttH") != std::string::npos) {
        sample_id = 2;
    } else if (sample.find("/DY") != std::string::npos) {
        sample_id = 3;
    } else if (sample.find("/Wjets") != std::string::npos) {
        sample_id = 4;
    } else if (sample.find("/SM_Higgs") != std::string::npos) {
        sample_id = 5;
    } else if (sample.find("/SM_Higgs") != std::string::npos) {
        sample_id = 6;
    } else if (sample.find("/VH") != std::string::npos) {
        sample_id = 7;
    } else if (sample.find("/VVV") != std::string::npos) {
        sample_id = 8;
    } else if (sample.find("/EWK") != std::string::npos) {
        sample_id = 9;
    } else if (sample.find("/VV") != std::string::npos) {
        sample_id = 10;
    } else{
        throw std::invalid_argument("Unrecognised sample: " + sample);
    }
}

int FileLooper::_sample2class_lookup(const int& sample) {
    if (sample < 0)  return 1;   // Signal
    if (sample == 0) return -1;  // Collider data
    return 0;                    // Background
}

bool FileLooper::_accept_evt(const int& region, const bool& syst_unc, const int& jet_cat, const bool& cut, const int& class_id) {
    if (_apply_cut && cut) return false;  // Require cut and cut failed
    if (!_inc_data && class_id == -1) return false; // Don't inlclude data and event is data
    if (!_inc_other_regions && region != 0) return false;  //Don't include other regions and event is not SS Iso
    if (!_inc_unc && !syst_unc) return false;  //Don't systematicss and event is a systematic
    if (!_inc_all_jets && jet_cat == 0) return false;  // Only use inference category jets and event is non-inference category
    return true;
}

unsigned long long int FileLooper::_get_strat_key(const int& sample, const int& klambda, const int& res_mass, const int& jet_cat, const int& region,
                                                  const int& spin, const int& syst_unc, const int& cut) {
    unsigned long long int strat_key = std::pow(2,  sample)*
                                       std::pow(3,  res_mass)*
                                       std::pow(5,  klambda)*
                                       std::pow(7,  jet_cat)*
                                       std::pow(11, region)*
                                       std::pow(13, spin)*
                                       std::pow(17, syst_unc)*
                                       std::pow(19, cut);
    return strat_key;
}