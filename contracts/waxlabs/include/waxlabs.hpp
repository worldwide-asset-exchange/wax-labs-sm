// WAX Labs is a Worker Proposal System for the WAX Blockchain Network.
//
// @author Craig Branscom
// @contract waxlabs
// @version v0.1.0

#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>
#include <eosio/action.hpp>

using namespace std;
using namespace eosio;

//approved treasuries: VOTE
//categories: marketing, apps, developers, education, games
//proposal statuses: drafting, submitted, approved, voting, inprogress, failed, cancelled, completed
//deliverable statuses: drafting, submitted, reported, accepted, rejected, claimed

CONTRACT waxlabs : public contract
{
    public:

    waxlabs(name self, name code, datastream<const char*> ds) : contract(self, code, ds) {}
    ~waxlabs() {}

    static constexpr symbol WAX_SYM = symbol("WAX", 8);
    static constexpr symbol VOTE_SYM = symbol("VOTE", 8);

    const uint8_t MAX_DELIVERABLES = 20;
    ACTION clearconf();
    // ACTION addconf();

    ACTION clear(uint64_t id);

    //======================== config actions ========================

    //initialize the contract
    //pre: config table not initialized
    //auth: self
    ACTION init(string contract_name, string contract_version, name initial_admin);

    //set contract version
    //auth: admin_acct
    ACTION setversion(string new_version);

    //set new admin
    //pre: new_admin account exists
    //auth: admin_acct
    ACTION setadmin(name new_admin);

    //set new vote duration
    //auth: admin_acct
    ACTION setduration(uint32_t new_vote_duration);

    //add a new proposal category
    //pre: new_category not in categories list
    //auth: admin_acct
    ACTION addcategory(name new_category);

    //remove a proposal category (existing proposals in category are not affected)
    //pre: category_name exists in categories list
    //auth: admin_acct
    ACTION rmvcategory(name category_name);

    //======================== proposal actions ========================

    //draft a new wax labs proposal
    //pre: config.available_funds >= total_requested_funds, valid category
    //auth: proposer
    ACTION draftprop(string title, string description, string content, name proposer, 
        name category, asset total_requested_funds);

    //edit a proposal draft
    //pre: proposal.status == drafting
    //auth: proposer
    ACTION editprop(uint64_t proposal_id, optional<string> title, 
        optional<string> description, optional<string> content, optional<name> category);

    //edit a proposal draft
    //pre: proposal.status == drafting
    //auth: proposer
    ACTION editprop(uint64_t proposal_id, optional<string> title, 
        optional<string> description, optional<string> content, optional<name> category);

    //submit a proposal draft for admin approval
    //pre: proposal.status == drafting
    //post: proposal.status == submitted
    //auth: proposer
    ACTION submitprop(uint64_t proposal_id);

    //approve or reject a submitted proposal
    //pre: proposal.status == submitted
    //post: proposal.status == approved if approved, proposal.status == failed if rejected
    //auth: admin_acct
    ACTION reviewprop(uint64_t proposal_id, bool approve, string memo);
    
    //begin voting period on a proposal
    //pre: proposal.status == drafting
    //auth: proposer
    ACTION beginvoting(uint64_t proposal_id, name ballot_name);

    //end voting period on a proposal and receive ballot results
    //pre: proposal.status == voting, now > decide::ballot.end_time
    //post: proposal.status == inprogress if passed, proposal.status == failed if failed
    //auth: proposer or admin_acct
    ACTION endvoting(uint64_t proposal_id);

    //set a reviewer for a deliverable
    //auth: admin_acct
    ACTION setreviewer(uint64_t proposal_id, uint64_t deliverable_id, name new_reviewer);

    //cancel proposal
    //pre: proposal.status == submitted || approved || voting
    //post: proposal.status == cancelled
    //auth: proposer
    ACTION cancelprop(uint64_t proposal_id, string memo);

    //delete proposal in terminal state
    //pre: proposal.status == failed || cancelled || completed
    //auth: proposer or admin_acct
    ACTION deleteprop(uint64_t proposal_id);

    //temporary action that wipes the RAM tables
    // auth: _self
    ACTION wipeprops(uint32_t count);
    
    //======================== deliverable actions ========================

    //adds a new deliverable to a proposal
    //pre: proposal.status == drafting
    //auth: proposer
    ACTION newdeliv(uint64_t proposal_id, uint64_t deliverable_id, asset requested_amount, name recipient);

    //remove a milestone from a proposal
    //pre: proposal.status == drafting
    //auth: proposer
    ACTION rmvdeliv(uint64_t proposal_id, uint64_t deliverable_id);

    //edit the requested amount for a deliverable
    //pre: proposal.status == drafting
    //post: sum amount of all deliverables == proposal.total_requested_funds
    //auth: proposer
    ACTION editdeliv(uint64_t proposal_id, uint64_t deliverable_id, asset new_requested_amount, name new_recipient);

    //submit a deliverable report
    //pre: proposal.status == inprogress, deliverable.status == inprogress
    //auth: proposer
    ACTION submitreport(uint64_t proposal_id, uint64_t deliverable_id, string report);

    //review a deliverable
    //pre: proposal.status == inprogress, deliverable.status == reported
    //auth: proposal.reviewer
    ACTION reviewdeliv(uint64_t proposal_id, uint64_t deliverable_id, bool accept, string memo);

    //claim deliverable funding
    //pre: proposal.status == inprogress, deliverable.status == accepted
    //auth: proposer or recipient
    ACTION claimfunds(uint64_t proposal_id, uint64_t deliverable_id);

    //======================== profile actions ========================

    //create a new profile
    //auth: wax_account
    ACTION newprofile(name wax_account, string full_name, string country, string bio, 
        string image_url, string website, string contact);

    //edit an existing profile
    //auth: wax_account
    ACTION editprofile(name wax_account, string full_name, string country, string bio, 
        string image_url, string website, string contact);

    //remove a profile
    //auth: wax_account or admin_acct
    ACTION rmvprofile(name wax_account);

    //======================== account actions ========================

    //withdraw from account balance
    //pre: account.balance >= quantity
    //auth: account_owner
    ACTION withdraw(name account_owner, asset quantity);

    //delete an account (allows contract to reclaim RAM)
    //pre: account.balance == 0
    //auth: account_name or admin_acct
    ACTION deleteacct(name account_name);

    //======================== notification handlers ========================

    //catches transfer notification from eosio.token
    [[eosio::on_notify("eosio.token::transfer")]]
    void catch_transfer(name from, name to, asset quantity, string memo);

    //catches broadcast notification from decide
    [[eosio::on_notify("decide::broadcast")]]
    void catch_broadcast(name ballot_name, map<name, asset> final_results, uint32_t total_voters);

    //======================== functions ========================

    //subtracts amount from balance
    void sub_balance(name account_owner, asset quantity);

    //adds amount to balance
    void add_balance(name account_owner, asset quantity);

    //returns true if category is valid
    bool valid_category(name category);

    //returns true if vote passed quorum threshold
    // bool did_pass_quorum_thresh();

    //returns true if vote passed yes threshold
    // bool did_pass_yes_thresh();

    //======================== contract tables ========================

    //config table
    //scope: self
    TABLE config {
        string contract_name; //name of contract
        string contract_version; //semver compliant contract version
        name admin_acct; //account that can approve proposals for voting
        name admin_auth = name("active"); //required permission for admin actions
        // uint64_t last_proposal_id; //last proposal id created
        asset available_funds = asset(0, WAX_SYM); //total available funding for proposals
        asset reserved_funds = asset(0, WAX_SYM); //total funding reserved by approved proposals
        asset deposited_funds = asset(0, WAX_SYM); //total deposited funds made by accounts
        asset paid_funds = asset(0, WAX_SYM); //total lifetime funding paid
        uint32_t vote_duration = 1'209'600; //length of voting period on a proposal in seconds (default is 14 days)
        double quorum_threshold = 10.0; //percent of votes to pass quorum
        double yes_threshold = 65.0; //percent of yes votes to approve
        asset min_requested = asset(1000'00000000, WAX_SYM); //minimum total reqeuested amount for proposals (default is 1k WAX)
        asset max_requested = asset(500000'00000000, WAX_SYM); //maximum total reqeuested amount for proposals (default is 500k WAX)
        vector<name> categories = { name("marketing"), name("apps"), name("developers"), name("education"), name("games") }; //list of approved proposal categories

        EOSLIB_SERIALIZE(config, (contract_name)(contract_version)(admin_acct)(admin_auth)
            (available_funds)(reserved_funds)(deposited_funds)(paid_funds)
            (vote_duration)(quorum_threshold)(yes_threshold)
            (min_requested)(max_requested)(categories))
    };
    typedef singleton<name("config"), config> config_singleton;

    //proposals table
    //scope: self
    TABLE proposal {
        uint64_t proposal_id; //unique id of proposal
        name proposer; //account name making proposal
        name category; //marketing, apps, developers, education
        name status = name("drafting"); //drafting, submitted, approved, voting, inprogress, failed, cancelled, completed
        name ballot_name = name(0); //name of decide ballot in voting phase (blank until voting begins)
        string title; //proposal title
        string description; //short tweet-length description
        string content; //link to full proposal content
        // string image_url; //link to image url
        // string estimated_time; //estimated time to completion (in days)
        asset total_requested_funds; //total funds requested
        asset remaining_funds = asset(0, WAX_SYM); //total remaining funds from total (set to total when approved)
        uint8_t deliverables; //total number of deliverables on project
        uint8_t deliverables_completed = 0; //total deliverables accepted by reviewer and claimed
        name reviewer = name(0); //account name reviewing the deliverables (blank until reviewer selected)
        map<name, asset> ballot_results = { { name("yes"), asset(0, VOTE_SYM) }, { name("no"), asset(0, VOTE_SYM) } }; //final ballot results from decide

        uint64_t primary_key() const { return proposal_id; }
        uint64_t by_proposer() const { return proposer.value; }
        uint64_t by_category() const { return category.value; }
        uint64_t by_status() const { return status.value; }
        uint64_t by_ballot() const { return ballot_name.value; }
        EOSLIB_SERIALIZE(proposal, (proposal_id)(proposer)(category)(status)(ballot_name)
            (title)(description)(content)(total_requested_funds)(remaining_funds)
            (deliverables)(deliverables_completed)(reviewer)(ballot_results))
    };
    typedef multi_index<name("proposals"), proposal,
        indexed_by<name("byproposer"), const_mem_fun<proposal, uint64_t, &proposal::by_proposer>>,
        indexed_by<name("bycategory"), const_mem_fun<proposal, uint64_t, &proposal::by_category>>,
        indexed_by<name("bystatus"), const_mem_fun<proposal, uint64_t, &proposal::by_status>>,
        indexed_by<name("byballot"), const_mem_fun<proposal, uint64_t, &proposal::by_ballot>>
    > proposals_table;

    //deliverables table
    //scope: proposal_id
    TABLE deliverable {
        uint64_t deliverable_id; //deliverable id
        name status = name("drafting"); //drafting, submitted, reported, accepted, rejected, claimed
        asset requested; //amount requested for deliverable
        name recipient; //account that will receive the funds
        string report = ""; //raw text or link to report for deliverable
        time_point_sec review_time = time_point_sec(0); //time of review (set to genesis date by default)

        uint64_t primary_key() const { return deliverable_id; }
        EOSLIB_SERIALIZE(deliverable, (deliverable_id)(status)(requested)
            (recipient)(report)(review_time))
    };
    typedef multi_index<name("deliverables"), deliverable> deliverables_table;

    //profiles table
    //scope: self
    TABLE profile {
        name wax_account;
        string full_name;
        string country;
        string bio;
        string image_url;
        string website;
        string contact;

        uint64_t primary_key() const { return wax_account.value; }
        EOSLIB_SERIALIZE(profile, (wax_account)(full_name)(country)(bio)
            (image_url)(website)(contact))
    };
    typedef multi_index<name("profiles"), profile> profiles_table;

    //accounts table
    //scope: account_name.value
    TABLE account {
        asset balance;

        uint64_t primary_key() const { return balance.symbol.code().raw(); }
        EOSLIB_SERIALIZE(account, (balance))
    };
    typedef multi_index<name("accounts"), account> accounts_table;

    //wax decide treasury
    //scope: DECIDE.value
    struct treasury {
        asset supply;
        asset max_supply;
        name access;
        name manager;
        string title;
        string description;
        string icon;
        uint32_t voters;
        uint32_t delegates;
        uint32_t committees;
        uint32_t open_ballots;
        bool locked;
        name unlock_acct;
        name unlock_auth;
        map<name, bool> settings;

        uint64_t primary_key() const { return supply.symbol.code().raw(); }
        EOSLIB_SERIALIZE(treasury, 
            (supply)(max_supply)(access)(manager)
            (title)(description)(icon)
            (voters)(delegates)(committees)(open_ballots)
            (locked)(unlock_acct)(unlock_auth)(settings))
    };
    typedef multi_index<name("treasuries"), treasury> treasuries_table;

};
