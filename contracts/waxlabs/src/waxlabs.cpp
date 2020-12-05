#include "../include/waxlabs.hpp"

// ACTION waxlabs::clear(uint64_t id)
// {
//     require_auth(get_self());
//     proposals_table proposals(get_self(), get_self().value);
//     auto& prop = proposals.get(id, "proposal not found");
//     proposals.erase(prop);
// }

//======================== config actions ========================

ACTION waxlabs::init(string contract_name, string contract_version, name initial_admin)
{
    //authenticate
    require_auth(get_self());
    
    //open config singleton
    config_singleton configs(get_self(), get_self().value);

    //validate
    check(!configs.exists(), "contract already initialized");
    check(is_account(initial_admin), "initial admin account doesn't exist");

    //initialize
    config initial_conf;
    initial_conf.contract_name = contract_name;
    initial_conf.contract_version = contract_version;
    initial_conf.admin_acct = initial_admin;

    //set initial config
    configs.set(initial_conf, get_self());
}

ACTION waxlabs::setversion(string new_version)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //authenticate
    require_auth(conf.admin_acct);

    //change contract version
    conf.contract_version = new_version;

    //set new config
    configs.set(conf, get_self());
}

ACTION waxlabs::setadmin(name new_admin)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //authenticate
    require_auth(conf.admin_acct);

    //validate
    check(is_account(new_admin), "new admin account doesn't exist");

    //change admin
    conf.admin_acct = new_admin;

    //set new config
    configs.set(conf, get_self());
}

ACTION waxlabs::setduration(uint32_t new_vote_duration)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //authenticate
    require_auth(conf.admin_acct);

    //validate
    check(new_vote_duration > 0, "new vote duration must be greater than zero");

    //change vote duration
    conf.vote_duration = new_vote_duration;

    //set new config
    configs.set(conf, get_self());
}

ACTION waxlabs::addcategory(name new_category)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //authenticate
    require_auth(conf.admin_acct);

    //initialize
    auto cat_itr = std::find(conf.categories.begin(), conf.categories.end(), new_category);

    //validate
    check(cat_itr == conf.categories.end(), "category name already exists");

    //add new category to categories list
    conf.categories.push_back(new_category);

    //set new config
    configs.set(conf, get_self());
}

ACTION waxlabs::rmvcategory(name category_name)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //authenticate
    require_auth(conf.admin_acct);

    //initialize
    auto cat_itr = std::find(conf.categories.begin(), conf.categories.end(), category_name);

    //validate
    check(cat_itr != conf.categories.end(), "category name not found");

    //erase category from categories list
    conf.categories.erase(cat_itr);

    //set new config
    configs.set(conf, get_self());
}

//======================== proposal actions ========================

ACTION waxlabs::draftprop(string title, string description, string content, name proposer, 
    name category, asset total_requested_funds)
{
    //authenticate
    require_auth(proposer);

    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //open profiles table, get profile
    profiles_table profiles(get_self(), get_self().value);
    auto& prof = profiles.get(proposer.value, "profile not found");

    //initialize
    auto cat_itr = std::find(conf.categories.begin(), conf.categories.end(), category);

    //validate
    check(total_requested_funds <= conf.max_requested, "requested amount exceeds allowed maximum requested amount");
    check(total_requested_funds >= conf.min_requested, "requested amount is less than minimum requested amount");
    check(total_requested_funds.amount > 0, "requested amount must be greater than zero");
    check(total_requested_funds.symbol == WAX_SYM, "requested amount must be denominated in WAX");
    check(cat_itr != conf.categories.end(), "invalid category");

    //open proposals table
    proposals_table proposals(get_self(), get_self().value);

    //initialize
    uint64_t new_proposal_id = proposals.available_primary_key();

    //create new proposal
    //ram payer: contract
    proposals.emplace(get_self(), [&](auto& col) {
        col.proposal_id = new_proposal_id;
        col.proposer = proposer;
        col.category = category;
        col.title = title;
        col.description = description;
        col.content = content;
        col.total_requested_funds = total_requested_funds;
        col.deliverables = 0;
    });
}

ACTION waxlabs::editprop(uint64_t proposal_id, optional<string> title, 
    optional<string> description, optional<string> content, optional<name> category)
{
    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //authenticate
    require_auth(prop.proposer);

    //validate
    check(prop.status == name("drafting"), "proposal must be in drafting state to edit");

    //assign
    string new_title = (title) ? *title : prop.title;
    string new_desc = (description) ? *description : prop.description;
    string new_content = (content) ? *content : prop.content;
    name new_category = (category) ? *category : prop.category;

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.category = new_category;
        col.title = new_title;
        col.description = new_desc;
        col.content = new_content;
    });
}

ACTION waxlabs::submitprop(uint64_t proposal_id)
{
    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //authenticate
    require_auth(prop.proposer);

    //validate
    check(prop.status == name("drafting"), "proposal must be in drafting state to submit");

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.status = name("submitted");
    });

}

ACTION waxlabs::reviewprop(uint64_t proposal_id, bool approve, string memo)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //authenticate
    require_auth(conf.admin_acct);

    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //validate
    check(prop.status == name("submitted"), "proposal must be in submitted state to review");

    //if admin approved
    if (approve) {
        //update proposal to approved
        proposals.modify(prop, same_payer, [&](auto& col) {
            col.status = name("approved");
        });
    } else {
        //update proposal to failed
        proposals.modify(prop, same_payer, [&](auto& col) {
            col.status = name("failed");
        });
    }
}

ACTION waxlabs::beginvoting(uint64_t proposal_id, name ballot_name)
{
    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //authenticate
    require_auth(prop.proposer);

    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //initialize
    asset newballot_fee = asset(1000000000, WAX_SYM); //10 WAX  TODO: get from decide config table
    time_point_sec now = time_point_sec(current_time_point());
    time_point_sec ballot_end_time = now + conf.vote_duration;
    vector<name> ballot_options = { name("yes"), name("no") };

    //charge proposal fee and newballot_fee
    sub_balance(prop.proposer, newballot_fee);

    //validate
    check(prop.status == "approved"_n, "proposal must be approved by admin to begin voting");
    check(prop.total_requested_funds <= conf.max_requested, "total requested is more than maximum allowed");
    check(conf.deposited_funds >= newballot_fee, "not enough deposited funds");

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.status = name("voting");
        col.ballot_name = ballot_name;
    });

    //update and set config
    conf.deposited_funds -= newballot_fee;
    configs.set(conf, get_self());

    //send inline transfer to pay for newballot fee
    action(permission_level{get_self(), name("active")}, name("eosio.token"), name("transfer"), make_tuple(
        get_self(), //from
        name("decide"), //to
        newballot_fee, //quantity
        string("Decide New Ballot Fee Payment") //memo
    )).send();

    //send inline newballot to decide
    action(permission_level{get_self(), name("active")}, name("decide"), name("newballot"), make_tuple(
        ballot_name, //ballot_name
        name("proposal"), //category
        get_self(), //publisher
        VOTE_SYM, //treasury_symbol
        name("1token1vote"), //voting_method
        ballot_options //initial_options
    )).send();
    
    //send inline editdetails to decide
    action(permission_level{get_self(), name("active")}, name("decide"), name("editdetails"), make_tuple(
        ballot_name, //ballot_name
        prop.title, //title
        prop.description, //description
        prop.content //content
    )).send();

    //toggle ballot votestake on (default is off)
    action(permission_level{get_self(), name("active")}, name("decide"), name("togglebal"), make_tuple(
        ballot_name, //ballot_name
        name("votestake") //setting_name
    )).send();
    
    //send inline openvoting to decide
    action(permission_level{get_self(), name("active")}, name("decide"), name("openvoting"), make_tuple(
        ballot_name, //ballot_name
        ballot_end_time //end_time
    )).send();
}

ACTION waxlabs::endvoting(uint64_t proposal_id)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //authenticate
    check(has_auth(prop.proposer) || has_auth(conf.admin_acct), "requires proposer or admin to authenticate");

    //validate
    check(prop.status == name("voting"), "proposal must be in voting state");

    //send inline closevoting to decide
    action(permission_level{get_self(), name("active")}, name("decide"), name("closevoting"), make_tuple(
        prop.ballot_name, //ballot_name
        true //broadcast
    )).send();

    //NOTE: results processed in catch_broadcast()

}

ACTION waxlabs::setreviewer(uint64_t proposal_id, uint64_t deliverable_id, name new_reviewer)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //authenticate
    require_auth(conf.admin_acct);

    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //validate
    check(is_account(new_reviewer), "new reviewer account doesn't exist");

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.reviewer = new_reviewer;
    });
}

ACTION waxlabs::cancelprop(uint64_t proposal_id, string memo)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //authenticate
    check(has_auth(prop.proposer) || has_auth(conf.admin_acct), "requires proposer or admin to authenticate");

    //initialzie
    name initial_status = prop.status;

    //validate
    check(prop.status == name("drafting") || prop.status == name("submitted") || 
        prop.status == name("approved") || prop.status == name("voting"), 
        "proposal must be in submitted, approved, or voting stages to cancel");

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.status = name("cancelled");
    });
    
    //reject all deliverables
    deliverables_table deliverables(get_self(), proposal_id);
    auto deliv_iter = deliverables.begin();
    while( deliv_iter != deliverables.end() ) {
        deliverables.modify(*deliv_iter, same_payer, [&](auto& col) {
            col.status = name("rejected");
        });
        deliv_iter++;
    }

    //if not in drafting mode
    if (initial_status == name("voting")) {
        //send inline cancelballot to decide
        action(permission_level{get_self(), name("active")}, name("decide"), name("cancelballot"), make_tuple(
            prop.ballot_name, //ballot_name
            string("WAX Labs Proposal Cancellation") //memo
        )).send();
    }
}

ACTION waxlabs::deleteprop(uint64_t proposal_id)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //authenticate
    check(has_auth(prop.proposer) || has_auth(conf.admin_acct), "requires proposer or admin to authenticate");

    //validate
    check(prop.status == name("failed") || prop.status == name("cancelled") || prop.status == name("completed"), 
        "proposal must be failed, cancelled, or completed to delete");

    //if remaining funds greater than zero
    if (prop.remaining_funds.amount > 0) {
        //add remaining funds to available funds
        conf.reserved_funds -= prop.remaining_funds;
        conf.available_funds += prop.remaining_funds;

        //update config
        configs.set(conf, get_self());
    }
    
    //erase each deliverable
    deliverables_table deliverables(get_self(), proposal_id);
    auto deliv_iter = deliverables.begin();
    while( deliv_iter != deliverables.end() ) {
        deliv_iter = deliverables.erase(deliv_iter);
    }

    //erase proposal
    proposals.erase(prop);
}

//======================== deliverable actions ========================

ACTION waxlabs::newdeliv(uint64_t proposal_id, uint64_t deliverable_id, asset requested_amount, name recipient)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");
    check(prop.deliverables < MAX_DELIVERABLES, "too many deliverables");

    //authenticate
    require_auth(prop.proposer);

    //validate
    check(prop.status == name("drafting"), "proposal must be in drafting state to add deliverable");
    check(requested_amount.amount > 0, "must request a positive amount");
    check(prop.total_requested_funds + requested_amount <= conf.max_requested, "total requested funds ablove allowed maximum per proposal");
    check(is_account(recipient), "recipient account doesn't exist");

    //add new deliverable
    //ram payer: contract
    deliverables_table deliverables(get_self(), proposal_id);
    deliverables.emplace(get_self(), [&](auto& col) {
        col.deliverable_id = deliverable_id;
        col.requested = requested_amount;
        col.recipient = recipient;
    });

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.total_requested_funds += requested_amount;
        col.deliverables += 1;
    });
}

ACTION waxlabs::rmvdeliv(uint64_t proposal_id, uint64_t deliverable_id)
{
    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //authenticate
    require_auth(prop.proposer);

    //open deliverables table, get last deliverable
    deliverables_table deliverables(get_self(), proposal_id);
    auto& deliv = deliverables.get(deliverable_id, "deliverable not found");

    //validate
    check(prop.status == name("drafting"), "proposal must be in drafting state to remove deliverable");
    check(prop.deliverables > 1, "proposal must have at least one deliverable");
    check(prop.total_requested_funds - deliv.requested > asset(0, WAX_SYM), "total requested amount cannot be at or below zero");

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.total_requested_funds -= deliv.requested;
        col.deliverables -= 1;
    });

    //erase deliverable
    deliverables.erase(deliv);
}

ACTION waxlabs::editdeliv(uint64_t proposal_id, uint64_t deliverable_id, asset new_requested_amount, name new_recipient)
{
    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //authenticate
    require_auth(prop.proposer);

    //open deliverables table, get deliverable
    deliverables_table deliverables(get_self(), proposal_id);
    auto& deliv = deliverables.get(deliverable_id, "deliverable not found");

    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //initialize
    asset request_delta = new_requested_amount - deliv.requested;
    asset new_total_requested = prop.total_requested_funds + request_delta;

    //validate
    check(prop.status == name("drafting"), "proposal must be in drafting state to edit deliverable");
    check(deliv.status == name("drafting"), "deliverable must be in drafting state to edit");
    check(new_requested_amount.amount > 0, "must request a positive amount");
    check(new_total_requested.amount > 0, "total requested funds must be above zero");
    check(new_total_requested <= conf.max_requested, "total requested funds must be at or below allowed maximum");
    check(is_account(new_recipient), "new recipient account doesn't exist");

    //update deliverable
    deliverables.modify(deliv, same_payer, [&](auto& col) {
        col.requested = new_requested_amount;
        col.recipient = new_recipient;
    });

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.total_requested_funds = new_total_requested;
    });
}

ACTION waxlabs::submitreport(uint64_t proposal_id, uint64_t deliverable_id, string report)
{
    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //authenticate
    require_auth(prop.proposer);

    //open deliverables table, get deliverable
    deliverables_table deliverables(get_self(), proposal_id);
    auto& deliv = deliverables.get(deliverable_id, "deliverable not found");

    //validate
    check(prop.status == name("inprogress"), "must submit report when proposal is in progress");
    check(deliv.status == name("inprogress") || deliv.status == name("rejected"), 
        "deliverable must be in progress or rejected to submit/resubmit report");
    check(report != "", "report cannot be empty");

    //update deliverable
    deliverables.modify(deliv, same_payer, [&](auto& col) {
        col.status = name("reported");
        col.report = report;
    });
}

ACTION waxlabs::reviewdeliv(uint64_t proposal_id, uint64_t deliverable_id, bool accept, string memo)
{
    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //authenticate
    require_auth(prop.reviewer);

    //open deliverables table, get deliverable
    deliverables_table deliverables(get_self(), proposal_id);
    auto& deliv = deliverables.get(deliverable_id, "deliverable not found");

    //validate
    check(prop.status == name("inprogress"), "proposal must be in progress to review deliverable");
    check(deliv.status == name("reported"), "deliverable must be reported to review");

    //if accepted
    if (accept) {
        //update deliverable
        deliverables.modify(deliv, same_payer, [&](auto& col) {
            col.status = name("accepted");
            col.review_time = time_point_sec(current_time_point());
        });
    } else {
        //update deliverable
        deliverables.modify(deliv, same_payer, [&](auto& col) {
            col.status = name("rejected");
            col.review_time = time_point_sec(current_time_point());
        });
    }
}

ACTION waxlabs::claimfunds(uint64_t proposal_id, uint64_t deliverable_id)
{
    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //open deliverables table, get deliverable
    deliverables_table deliverables(get_self(), proposal_id);
    auto& deliv = deliverables.get(deliverable_id, "deliverable not found");

    //authenticate
    check(has_auth(prop.proposer) || has_auth(deliv.recipient), "claiming funds requires authentication from proposer or recipient");

    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //validate
    check(prop.status == name("inprogress"), "proposal must be in progress to claim funds");
    check(deliv.status == name("accepted"), "deliverable must be accepted by reviewer to claim funds");

    //update deliverable
    deliverables.modify(deliv, same_payer, [&](auto& col) {
        col.status = name("claimed");
    });

    //initialize
    name new_prop_status = prop.status;

    //if last deliverable
    if (prop.deliverables_completed == (prop.deliverables - 1)) {
        new_prop_status = name("completed");
    }

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.status = new_prop_status;
        col.remaining_funds -= deliv.requested;
        col.deliverables_completed += 1;
    });

    //update and set conf
    conf.reserved_funds -= deliv.requested;
    conf.paid_funds += deliv.requested;
    configs.set(conf, get_self());

    //move requested funds to recipient account
    add_balance(deliv.recipient, deliv.requested);
}

//======================== profile actions ========================

ACTION waxlabs::newprofile(name wax_account, string full_name, string country, string bio, 
    string image_url, string website, string contact)
{
    //authenticate
    require_auth(wax_account);

    //open profiles table, find profile
    profiles_table profiles(get_self(), get_self().value);
    auto prof_itr = profiles.find(wax_account.value);

    //validate
    check(prof_itr == profiles.end(), "profile already exists");

    //create new profile
    //ram payer: contract
    profiles.emplace(get_self(), [&](auto& col) {
        col.wax_account = wax_account;
        col.full_name = full_name;
        col.country = country;
        col.bio = bio;
        col.image_url = image_url;
        col.website = website;
        col.contact = contact;
    });
}

ACTION waxlabs::editprofile(name wax_account, string full_name, string country, string bio, 
    string image_url, string website, string contact)
{
    //authenticate
    require_auth(wax_account);

    //open profiles table, find profile
    profiles_table profiles(get_self(), get_self().value);
    auto& prof = profiles.get(wax_account.value, "profile not found");

    //update profile
    profiles.modify(prof, same_payer, [&](auto& col) {
        col.full_name = full_name;
        col.country = country;
        col.bio = bio;
        col.image_url = image_url;
        col.website = website;
        col.contact = contact;
    });
}

ACTION waxlabs::rmvprofile(name wax_account)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //open profiles table, find profile
    profiles_table profiles(get_self(), get_self().value);
    auto& prof = profiles.get(wax_account.value, "profile not found");

    //authenticate
    check(has_auth(prof.wax_account) || has_auth(conf.admin_acct), "requires authentication from profile account or admin account");

    //erase profile
    profiles.erase(prof);
}

//======================== account actions ========================

ACTION waxlabs::withdraw(name account_name, asset quantity)
{
    //authenticate
    require_auth(account_name);

    //validate
    check(quantity.symbol == WAX_SYM, "must withdraw WAX");
    check(quantity.amount > 0, "must withdraw positive amount");

    //subtract balance from account
    sub_balance(account_name, quantity);

    //inline transfer
    action(permission_level{get_self(), name("active")}, name("eosio.token"), name("transfer"), make_tuple(
        get_self(), //from
        account_name, //to
        quantity, //quantity
        std::string("Wax Labs Withdrawal") //memo
    )).send();
}

ACTION waxlabs::deleteacct(name account_name)
{
    //authenticate
    require_auth(account_name);
    
    //open accounts table, get account
    accounts_table accounts(get_self(), account_name.value);
    auto& acct = accounts.get(WAX_SYM.code().raw(), "account not found");

    //validate
    check(acct.balance.amount == 0, "account must be empty to delete");

    //delete account
    accounts.erase(acct);
}

//======================== notification handlers ========================

void waxlabs::catch_transfer(name from, name to, asset quantity, string memo)
{
    //get initial receiver contract
    name rec = get_first_receiver();

    //if notification from eosio.token, transfer not from self, and WAX symbol
    if (rec == name("eosio.token") && from != get_self() && quantity.symbol == WAX_SYM) {
        //skips emplacement if memo is skip
        if (memo == std::string("skip")) { 
            return;
        }

        //open config singleton, get config
        config_singleton configs(get_self(), get_self().value);
        auto conf = configs.get();

        //adds to available funds
        if (memo == std::string("fund")) { 
            //update available funds
            conf.available_funds += quantity;
            configs.set(conf, get_self());
        } else {
            //open accounts table, search for account
            accounts_table accounts(get_self(), from.value);
            auto acct = accounts.find(WAX_SYM.code().raw());

            //emplace account if not found, update if exists
            if (acct == accounts.end()) {
                //make new account
                accounts.emplace(get_self(), [&](auto& col) {
                    col.balance = quantity;
                });
            } else {
                //update existing account
                accounts.modify(*acct, same_payer, [&](auto& col) {
                    col.balance += quantity;
                });

            }

            //update config funds
            conf.deposited_funds += quantity;
            configs.set(conf, get_self());
        }
    }
}

void waxlabs::catch_broadcast(name ballot_name, map<name, asset> final_results, uint32_t total_voters)
{
    //get initial receiver contract
    name rec = get_first_receiver();

    //if notification not from decide account
    if (rec != name("decide")) {
        return;
    }
        
    //open proposals table, get by ballot index, find proposal
    proposals_table proposals(get_self(), get_self().value);
    auto props_by_ballot = proposals.get_index<name("byballot")>();
    auto by_ballot_itr = props_by_ballot.lower_bound(ballot_name.value);

    //if proposal found
    if (by_ballot_itr != props_by_ballot.end()) {

        //open config singleton, get config
        config_singleton configs(get_self(), get_self().value);
        auto conf = configs.get();

        //open wax decide treasury table, get treasury
        treasuries_table treasuries(name("decide"), name("decide").value);
        auto& trs = treasuries.get(VOTE_SYM.code().raw(), "treasury not found");

        //validate
        check(by_ballot_itr->status == name("voting"), "proposal must be in voting state to end voting");

        //initialize
        name new_prop_status;
        asset new_remaining_funds = asset(0, WAX_SYM);
        asset total_votes = final_results[name("yes")] + final_results[name("no")];
        asset quorum_thresh = trs.supply * conf.quorum_threshold / 100;
        asset approve_thresh = total_votes * conf.yes_threshold / 100;

        //if total votes passed quorum thresh and yes votes passed approve thresh
        if (total_votes >= quorum_thresh && final_results["yes"_n] >= approve_thresh) {
            //validate
            check(conf.available_funds >= by_ballot_itr->total_requested_funds, "WAX Labs has insufficient available funds");

            //update config funds
            conf.available_funds -= by_ballot_itr->total_requested_funds;
            conf.reserved_funds += by_ballot_itr->total_requested_funds;
            configs.set(conf, get_self());

            //loop over all deliverables
            deliverables_table deliverables(get_self(), by_ballot_itr->proposal_id);
            auto deliv_iter = deliverables.begin();
            while( deliv_iter != deliverables.end() ) {
                deliverables.modify(*deliv_iter, same_payer, [&](auto& col) {
                    col.status = name("inprogress");
                });
                deliv_iter++;
            }

            //update proposal
            proposals.modify(*by_ballot_itr, same_payer, [&](auto& col) {
                col.status = name("inprogress");
                col.remaining_funds = by_ballot_itr->total_requested_funds;
            });
        } else {
            //update proposal
            proposals.modify(*by_ballot_itr, same_payer, [&](auto& col) {
                col.status = name("failed");
            });
        }
    }
}

//======================== functions ========================

void waxlabs::sub_balance(name account_owner, asset quantity)
{
    //open accounts table, get account
    accounts_table accounts(get_self(), account_owner.value);
    auto& acct = accounts.get(WAX_SYM.code().raw(), "sub_balance: account not found");

    //validate
    check(acct.balance >= quantity, "sub_balance: insufficient funds >>> needed: " + asset(quantity.amount - acct.balance.amount, WAX_SYM).to_string());

    //subtract quantity from balance
    accounts.modify(acct, same_payer, [&](auto& col) {
        col.balance -= quantity;
    });
}

void waxlabs::add_balance(name account_owner, asset quantity)
{
    //open accounts table, get account
    accounts_table accounts(get_self(), account_owner.value);
    auto& acct = accounts.get(WAX_SYM.code().raw(), "add_balance: account not found");

    //add quantity to balance
    accounts.modify(acct, same_payer, [&](auto& col) {
        col.balance += quantity;
    });
}

bool waxlabs::valid_category(name category)
{
    //check category name
    switch (category.value) {
        case (name("marketing").value):
            return true;
        case (name("apps").value):
            return true;
        case (name("developers").value):
            return true;
        case (name("education").value):
            return true;
        case (name("games").value):
            return true;
        default:
            return false;
    }
}
