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
//categories added by default: marketing, infra.tools, dev.tools, governance, other

CONTRACT waxlabs : public contract
{
    public:

    waxlabs(name self, name code, datastream<const char*> ds) : contract(self, code, ds) {}
    ~waxlabs() {}

    static constexpr symbol WAX_SYM = symbol("WAX", 8);
    static constexpr symbol VOTE_SYM = symbol("VOTE", 8);

    // Cost to be charged from balance, when draftprop() action is called.
    const asset DRAFT_COST = asset(100'00000000, WAX_SYM);

    const size_t  MAX_DELIVERABLES = 20;
    const size_t  MAX_CATEGORIES = 20;
    const size_t  MAX_TITLE_LEN = 64;
    const size_t  MAX_DESCR_LEN = 160;
    const size_t  MAX_BODY_LEN = 4096;
    const size_t  MAX_IMGURL_LEN = 256;
    const size_t  MAX_SMALL_DESC_LEN = 80;
    const size_t  MAX_ROAD_MAP_LEN = 2048;

    const uint64_t MAX_PROPOSAL_ID = 0xFFFFFFFF;

    enum class proposal_status : uint8_t {
        drafting    = 1,
        submitted   = 2,
        approved    = 3,
        voting      = 4,
        inprogress  = 5,
        failed      = 6,
        cancelled   = 7,
        completed   = 8,
    };

    friend constexpr bool operator == ( const uint8_t& a, const proposal_status& b) {
        return a == static_cast<uint8_t>(b);
    }




    enum class deliverable_status : uint8_t {
        drafting = 1,
        inprogress = 2,
        reported = 3,
        accepted = 4,
        rejected = 5,
        claimed = 6,
    };

    friend constexpr bool operator == ( const uint8_t& a, const deliverable_status& b) {
        return a == static_cast<uint8_t>(b);
    }


    //======================== config actions ========================


    //temporary action that wipes the RAM tables
    // auth: _self
    ACTION wipestats();
    ACTION wipeprops(uint32_t count);

    ACTION wipedelvs(uint64_t proposal_id, uint32_t count);

    ACTION wipeconf();

    ACTION wipebodies(uint32_t count);
    ACTION wipeprofiles(uint32_t count);

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

    //add a new proposal category or remove it from deprecated list
    //pre: new_category not in categories list or in deprecated list
    //auth: admin_acct
    ACTION addcategory(name new_category);

    //add a proposal category to deprecated list
    //pre: category_name in categories list
    //auth: admin_acct
    ACTION rmvcategory(name category_name);

    //======================== proposal actions ========================

    //draft a new wax labs proposal
    //pre: valid category. category can't be in config cat_deprecated list.
    // estimated time must be greater than 0.
    // proposer must have a profile registered.
    // description, mdbody, image_url and title must not be bigger than max_len variables
    // account must have more than
    //auth: proposer
    ACTION draftprop(string title, string description, string mdbody, name proposer,
        string image_url, uint32_t estimated_time, name category, string road_map);

    //edit a proposal draft
    //pre: proposal.status == drafting
    //auth: proposer
    ACTION editprop(uint64_t proposal_id, optional<string> title,
        optional<string> description, optional<string> mdbody, optional<name> category,
        string image_url, uint32_t estimated_time, optional<string> road_map);

    //submit a proposal draft for admin approval
    //pre: proposal.status == drafting, reviewer is set.
    //post: proposal.status == submitted
    //auth: proposer
    ACTION submitprop(uint64_t proposal_id);

    //approve or reject a submitted proposal
    //pre: proposal.status == submitted
    //post: proposal.status == approved if approved, proposal.status == failed if rejected
    //auth: admin_acct
    ACTION reviewprop(uint64_t proposal_id, bool approve, string memo);


    //pass a proposal, without going through voting
    //pre: proposal.status == submitted
    //post: proposal.status == inprogress
    //auth: admin_acct
    ACTION skipvoting(uint64_t proposal_id, string memo);

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
    //pre: proposal.status == drafing || submitted || approved || voting
    //post: proposal.status == cancelled
    //auth: proposer
    ACTION cancelprop(uint64_t proposal_id, string memo);

    //delete proposal in terminal state
    //pre: proposal.status == failed || cancelled || completed
    //auth: proposer or admin_acct
    ACTION deleteprop(uint64_t proposal_id);

    //======================== deliverable actions ========================

    //adds a new deliverable to a proposal
    //pre: proposal.status == drafting
    //auth: proposer
    ACTION newdeliv(uint64_t proposal_id, uint64_t deliverable_id, asset requested_amount, name recipient, string small_description, uint32_t days_to_complete);

    //remove a milestone from a proposal
    //pre: proposal.status == drafting
    //auth: proposer
    ACTION rmvdeliv(uint64_t proposal_id, uint64_t deliverable_id);

    //edit the requested amount for a deliverable
    //pre: proposal.status == drafting
    //post: sum amount of all deliverables == proposal.total_requested_funds
    //auth: proposer
    ACTION editdeliv(uint64_t proposal_id, uint64_t deliverable_id, asset new_requested_amount, name new_recipient, string small_description, uint32_t days_to_complete);

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
        string image_url, string website, string contact, string group_name);

    //edit an existing profile
    //auth: wax_account
    ACTION editprofile(name wax_account, string full_name, string country, string bio,
        string image_url, string website, string contact, string group_name);

    //remove a profile
    //auth: wax_account or admin_acct
    ACTION rmvprofile(name wax_account);

    //======================== account actions ========================

    //withdraw from account balance
    //pre: account.balance >= quantity
    //auth: account_owner
    ACTION withdraw(name account_owner, asset quantity);

    //======================== notification handlers ========================

    //catches transfer notification from eosio.token
    [[eosio::on_notify("eosio.token::transfer")]]
    void catch_transfer(name from, name to, asset quantity, string memo);

    //catches broadcast notification from decide
    [[eosio::on_notify("decide::broadcast")]]
    void catch_broadcast(name ballot_name, map<name, asset> final_results, uint32_t total_voters);

    //======================== functions ========================

    // increments (creates if not found) stat count both total and current.
    void inc_stats_count(uint64_t key, string val_name);

    // decrements (fails if not found) stat count
    // by default only decrements current,
    // if dec_total is true also decrements total.
    void dec_stats_count(uint64_t key, bool dec_total = false);

    //subtracts amount from balance
    void sub_balance(name account_owner, asset quantity);

    //adds amount to balance
    void add_balance(name account_owner, asset quantity);

    //returns true if vote passed quorum threshold
    // bool did_pass_quorum_thresh();

    //returns true if vote passed yes threshold
    // bool did_pass_yes_thresh();

    //sets a comment for a proposal
    void set_pcomment(uint64_t proposal_id, string status_comment, name payer);

    //sets a comment for a deliverable
    void set_dcomment(uint64_t proposal_id, uint64_t deliverable_id, string status_comment, name payer);

    //======================== contract tables ========================

    //stat table
    //scope self
    TABLE stat {
        uint64_t            key;
        string              val_name;
        uint128_t           current_count = 0;
        uint128_t           total_count = 0;

        auto primary_key()const { return key; }

        EOSLIB_SERIALIZE(stat, (key)(val_name)(current_count)(total_count))
    };
    typedef eosio::multi_index<
        name("stats"), stat> stats;

    //config table
    //scope: self
    TABLE config {
        string contract_name; //name of contract
        string contract_version; //semver compliant contract version
        name admin_acct; //account that can approve proposals for voting
        name admin_auth = name("active"); //required permission for admin actions
        uint64_t last_proposal_id; //last proposal id created
        asset available_funds = asset(0, WAX_SYM); //total available funding for proposals
        asset reserved_funds = asset(0, WAX_SYM); //total funding reserved by approved proposals
        asset deposited_funds = asset(0, WAX_SYM); //total deposited funds made by accounts
        asset paid_funds = asset(0, WAX_SYM); //total lifetime funding paid
        uint32_t vote_duration = 1'209'600; //length of voting period on a proposal in seconds (default is 14 days)
        double quorum_threshold = 10.0; //percent of votes to pass quorum
        double yes_threshold = 50.0; //percent of yes votes to approve
        asset min_requested = asset(1000'00000000, WAX_SYM); //minimum total reqeuested amount for proposals (default is 1k WAX)
        asset max_requested = asset(500000'00000000, WAX_SYM); //maximum total reqeuested amount for proposals (default is 500k WAX)
        vector<name> categories = { name("marketing"), name("infra.tools"), name("dev.tools"), name("governance"), name("other") };
        vector<name> cat_deprecated; // list of categories unavailable for new proposals

        EOSLIB_SERIALIZE(config, (contract_name)(contract_version)(admin_acct)(admin_auth)(last_proposal_id)
            (available_funds)(reserved_funds)(deposited_funds)(paid_funds)
            (vote_duration)(quorum_threshold)(yes_threshold)
            (min_requested)(max_requested)(categories)(cat_deprecated))
    };
    typedef singleton<name("config"), config> config_singleton;

    //proposals table
    //scope: self
    TABLE proposal {
        uint64_t proposal_id; //unique id of proposal
        name proposer; //account name making proposal
        uint8_t category;
        uint8_t status = static_cast<uint8_t>(proposal_status::drafting);
        name ballot_name = name(0); //name of decide ballot in voting phase (blank until voting begins)
        string title; //proposal title
        string description; //short tweet-length description
        string image_url; //link to image url
        uint32_t estimated_time; //estimated time to completion (in days)
        asset total_requested_funds; //total funds requested
        asset remaining_funds = asset(0, WAX_SYM); //total remaining funds from total (set to total when approved)
        uint8_t deliverables; //total number of deliverables on project
        uint8_t deliverables_completed = 0; //total deliverables accepted by reviewer and claimed
        name reviewer = name(0); //account name reviewing the deliverables (blank until reviewer selected)
        map<name, asset> ballot_results = { { name("yes"), asset(0, VOTE_SYM) }, { name("no"), asset(0, VOTE_SYM) } }; //final ballot results from decide
        time_point_sec update_ts; // timestamp of latest proposal update
        time_point_sec vote_end_time; // vote_endtime that was passed to decide contract
        string road_map;

        uint64_t primary_key() const { return proposal_id; }

        // Upper 16 bits: status, category; lower 32 bits: proposal_id
        uint64_t by_status_and_category() const { return (((uint64_t)status << 56)|((uint64_t)category << 48)|proposal_id); }

        // Upper 16 bits: category, status; lower 32 bits: proposal_id
        uint64_t by_category_and_status() const { return (((uint64_t)category << 56)|((uint64_t)status << 48)|proposal_id); }

        // Upper 64 bits: proposer account; bits 63-56: status; bits 31-0: proposal_id
        uint128_t by_proposer() const { return (((uint128_t)proposer.value << 64)|((uint128_t)status << 56)|((uint128_t)proposal_id)); }

        // Upper 64 bits: reviewer account; bits 63-56: status; bits 31-0: proposal_id
        uint128_t by_reviewer() const { return (((uint128_t)reviewer.value << 64)|((uint128_t)status << 56)|((uint128_t)proposal_id)); }

        uint64_t by_ballot() const { return ballot_name.value; }

        uint64_t by_update_ts() const { return (((uint64_t)update_ts.sec_since_epoch() << 32)|proposal_id); }

        EOSLIB_SERIALIZE(proposal, (proposal_id)(proposer)(category)(status)(ballot_name)
            (title)(description)(image_url)(estimated_time)(total_requested_funds)(remaining_funds)
            (deliverables)(deliverables_completed)(reviewer)(ballot_results)
            (update_ts)(vote_end_time)(road_map))
    };
    typedef multi_index<name("proposals"), proposal,
        indexed_by<name("bystatcat"), const_mem_fun<proposal, uint64_t, &proposal::by_status_and_category>>,
        indexed_by<name("bycatstat"), const_mem_fun<proposal, uint64_t, &proposal::by_category_and_status>>,
        indexed_by<name("byproposer"), const_mem_fun<proposal, uint128_t, &proposal::by_proposer>>,
        indexed_by<name("byreviewer"), const_mem_fun<proposal, uint128_t, &proposal::by_reviewer>>,
        indexed_by<name("byballot"), const_mem_fun<proposal, uint64_t, &proposal::by_ballot>>,
        indexed_by<name("byupdatets"), const_mem_fun<proposal, uint64_t, &proposal::by_update_ts>>
    > proposals_table;

    //mdbody table
    //proposal content is stored in Markdown format in a separate table to save on deserialization costs
    TABLE mdbody {
        uint64_t proposal_id; //unique id of proposal
        string content; //content in Markdown format

        uint64_t primary_key() const { return proposal_id; }

        EOSLIB_SERIALIZE(mdbody, (proposal_id)(content))
    };
    typedef multi_index<name("mdbodies"), mdbody> mdbodies_table;

    //proposal comments table
    //proposal receives comments from the admin team during the reviews. The reviewer pays for RAM,
    //so the comment is stored in a separate table
    TABLE pcomment {
        uint64_t proposal_id; //unique id of proposal
        string status_comment; //content in Markdown format
        uint64_t primary_key() const { return proposal_id; }
        EOSLIB_SERIALIZE(pcomment, (proposal_id)(status_comment))
    };
    typedef multi_index<name("pcomments"), pcomment> pcomments_table;

    //deliverables table
    //scope: proposal_id
    TABLE deliverable {
        uint64_t deliverable_id; //deliverable id
        uint8_t status = static_cast<uint8_t>(deliverable_status::drafting);
        asset requested; //amount requested for deliverable
        name recipient; //account that will receive the funds
        string report = ""; //raw text or link to report for deliverable
        time_point_sec review_time = time_point_sec(0); //time of review (set to genesis date by default)
        string small_description;
        uint32_t days_to_complete;

        uint64_t primary_key() const { return deliverable_id; }
        EOSLIB_SERIALIZE(deliverable, (deliverable_id)(status)(requested)
            (recipient)(report)(review_time)(small_description)(days_to_complete))
    };
    typedef multi_index<name("deliverables"), deliverable> deliverables_table;

    //deliverable comments table
    //scope: proposal_id
    //proposal receives comments from the admin team during the reviews. The reviewer pays for RAM,
    //so the comment is stored in a separate table
    TABLE dcomment {
        uint64_t deliverable_id; //unique id of proposal
        string status_comment; //content in Markdown format
        uint64_t primary_key() const { return deliverable_id; }
        EOSLIB_SERIALIZE(dcomment, (deliverable_id)(status_comment))
    };
    typedef multi_index<name("dcomments"), dcomment> dcomments_table;


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
        string group_name;

        uint64_t primary_key() const { return wax_account.value; }
        EOSLIB_SERIALIZE(profile, (wax_account)(full_name)(country)(bio)
            (image_url)(website)(contact)(group_name))
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


/*
  Local Variables:
  mode: c++
  c-default-style: "linux"
  c-basic-offset: 4
  End:
*/
