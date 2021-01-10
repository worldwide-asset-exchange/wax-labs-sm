#include "../include/waxlabs.hpp"

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

    //check if it's a deprecated categpry
    auto depr_itr = std::find(conf.cat_deprecated.begin(), conf.cat_deprecated.end(), new_category);
    if (depr_itr != conf.cat_deprecated.end()) {
        conf.cat_deprecated.erase(depr_itr);
    }
    else {
        check(conf.categories.size() < MAX_CATEGORIES, "too many categories defined");
        auto cat_itr = std::find(conf.categories.begin(), conf.categories.end(), new_category);
        check(cat_itr == conf.categories.end(), "category name already exists");

        //add new category to categories list
        conf.categories.push_back(new_category);
    }

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

    //validate
    auto cat_itr = std::find(conf.categories.begin(), conf.categories.end(), category_name);
    check(cat_itr != conf.categories.end(), "category name not found");
    auto depr_itr = std::find(conf.cat_deprecated.begin(), conf.cat_deprecated.end(), category_name);
    check(depr_itr == conf.cat_deprecated.end(), "category name is already in deprecated list");

    //add category to deprecated list
    conf.cat_deprecated.push_back(category_name);

    //set new config
    configs.set(conf, get_self());
}

//======================== proposal actions ========================

ACTION waxlabs::draftprop(string title, string description, string mdbody, name proposer,
    string image_url, uint32_t estimated_time, name category)
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
    auto depr_itr = std::find(conf.cat_deprecated.begin(), conf.cat_deprecated.end(), category);

    //validate
    check(title.length() <= MAX_TITLE_LEN, "title string is too long");
    check(description.length() <= MAX_DESCR_LEN, "description string is too long");
    check(mdbody.length() <= MAX_BODY_LEN, "body string is too long");
    check(image_url.length() <= MAX_IMGURL_LEN, "image URL string is too long");
    check(cat_itr != conf.categories.end(), "invalid category");
    check(depr_itr == conf.cat_deprecated.end(), "this category name is deprecated");
    check(estimated_time > 0, "estimated time must be greater than zero");

    //subtract DRAFT_COST from account balance
    sub_balance(proposer, DRAFT_COST);

    size_t cat_pos = std::distance(conf.categories.begin(), cat_itr);

    //open tables
    proposals_table proposals(get_self(), get_self().value);
    mdbodies_table mdbodies(get_self(), get_self().value);

    //initialize
    uint64_t new_proposal_id = conf.last_proposal_id + 1;
    check(new_proposal_id <= MAX_PROPOSAL_ID, "too many proposals");

    // update last_proposal_id
    conf.last_proposal_id = new_proposal_id;
    configs.set(conf, get_self());

    //create new proposal 
    //ram payer: proposer
    proposals.emplace(proposer, [&](auto& col) {
        col.proposal_id = new_proposal_id;
        col.proposer = proposer;
        col.category = cat_pos;
        col.title = title;
        col.description = description;
        col.image_url = image_url;
        col.estimated_time = estimated_time;
        col.total_requested_funds = asset(0, WAX_SYM);
        col.deliverables = 0;
        col.update_ts = time_point_sec(current_time_point());
    });

    mdbodies.emplace(proposer, [&](auto& col) {
        col.proposal_id = new_proposal_id;
        col.content = mdbody;
    });
}

ACTION waxlabs::editprop(uint64_t proposal_id, optional<string> title,
    optional<string> description, optional<string> mdbody, optional<name> category,
    string image_url, uint32_t estimated_time)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //open tables, get proposal and body
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");
    mdbodies_table mdbodies(get_self(), get_self().value);
    auto& body = mdbodies.get(proposal_id, "proposal not found in mbodies");

    //authenticate
    require_auth(prop.proposer);

    //validate
    check(prop.status == proposal_status::drafting, "proposal must be in drafting state to edit");
    check(estimated_time > 0, "estimated time must be greater than zero");

    string new_title = prop.title;
    if (title) {
        new_title = *title;
        check(new_title.length() <= MAX_TITLE_LEN, "title string is too long");
    }

    string new_desc = prop.description;
    if (description) {
        new_desc = *description;
        check(new_desc.length() <= MAX_DESCR_LEN, "description string is too long");
    }

    string new_mdbody = body.content;
    if (mdbody) {
        new_mdbody = *mdbody;
        check(new_mdbody.length() <= MAX_BODY_LEN, "body string is too long");
    }

    uint8_t new_category = prop.category;
    if (category) {
        auto cat_itr = std::find(conf.categories.begin(), conf.categories.end(), *category);
        check(cat_itr != conf.categories.end(), "invalid category");
        auto depr_itr = std::find(conf.cat_deprecated.begin(), conf.cat_deprecated.end(), category);
        check(depr_itr == conf.cat_deprecated.end(), "this category name is deprecated");
        new_category = std::distance(conf.categories.begin(), cat_itr);
    }

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.category = new_category;
        col.title = new_title;
        col.description = new_desc;
        col.image_url = image_url;
        col.estimated_time = estimated_time;
        col.update_ts = time_point_sec(current_time_point());
    });

    mdbodies.modify(body, same_payer, [&](auto& col) {
        col.content = new_mdbody;
    });
}

ACTION waxlabs::submitprop(uint64_t proposal_id)
{
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //open proposals table, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");

    //authenticate
    require_auth(prop.proposer);

    //validate
    check(prop.status == proposal_status::drafting, "proposal must be in drafting state to submit");
    check(prop.deliverables >= 1, "proposal must have at least one deliverable to submit");
    check(prop.total_requested_funds >= conf.min_requested, "requested amount is less than minimum requested amount");
    check(prop.total_requested_funds <= conf.max_requested, "total requested is more than maximum allowed");

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.status = static_cast<uint8_t>(proposal_status::submitted);
        col.update_ts = time_point_sec(current_time_point());
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
    check(prop.status == proposal_status::submitted, "proposal must be in submitted state to review");

    //if admin approved
    if (approve) {
        //check if reviewer is set
        check(is_account(prop.reviewer), "reviewer account needs to be set before approving");
        //update proposal to approved
        proposals.modify(prop, same_payer, [&](auto& col) {
            col.status = static_cast<uint8_t>(proposal_status::approved);
            col.status_comment = memo;
            col.update_ts = time_point_sec(current_time_point());
        });
    } else {
        //update proposal to failed
        proposals.modify(prop, same_payer, [&](auto& col) {
            col.status = static_cast<uint8_t>(proposal_status::failed);
            col.status_comment = memo;
            col.update_ts = time_point_sec(current_time_point());
        });
    }
}

ACTION waxlabs::beginvoting(uint64_t proposal_id, name ballot_name)
{
    //open tables, get proposal
    proposals_table proposals(get_self(), get_self().value);
    auto& prop = proposals.get(proposal_id, "proposal not found");
    mdbodies_table mdbodies(get_self(), get_self().value);
    auto& body = mdbodies.get(proposal_id, "proposal not found in mbodies");

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
    check(prop.status == proposal_status::approved, "proposal must be approved by admin to begin voting");
    check(conf.deposited_funds >= newballot_fee, "not enough deposited funds");

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.status = static_cast<uint8_t>(proposal_status::voting);
        col.status_comment = "";
        col.ballot_name = ballot_name;
        col.update_ts = time_point_sec(current_time_point());
        col.vote_end_time = ballot_end_time;
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
        body.content //content
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
    check(prop.status == proposal_status::voting, "proposal must be in voting state");

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
        col.update_ts = time_point_sec(current_time_point());
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
    proposal_status initial_status = static_cast<proposal_status>(prop.status);

    //validate
    check(prop.status == proposal_status::drafting || prop.status == proposal_status::submitted ||
          prop.status == proposal_status::approved || prop.status == proposal_status::voting,
          "proposal must be in submitted, approved, or voting stages to cancel");

    //update proposal. RAM payer=self because we write the memo
    proposals.modify(prop, _self, [&](auto& col) {
        col.status = static_cast<uint8_t>(proposal_status::cancelled);
        col.status_comment = memo;
        col.update_ts = time_point_sec(current_time_point());
    });

    //reject all deliverables
    deliverables_table deliverables(get_self(), proposal_id);
    auto deliv_iter = deliverables.begin();
    while( deliv_iter != deliverables.end() ) {
        deliverables.modify(*deliv_iter, _self, [&](auto& col) {
            col.status = static_cast<uint8_t>(deliverable_status::rejected);
            col.status_comment = memo;
        });
        deliv_iter++;
    }

    //Decide has the ballot only when proposal is in voting state
    if (initial_status == proposal_status::voting) {
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
    mdbodies_table mdbodies(get_self(), get_self().value);
    auto& body = mdbodies.get(proposal_id, "proposal not found in mbodies");

    //authenticate
    check(has_auth(prop.proposer) || has_auth(conf.admin_acct), "requires proposer or admin to authenticate");

    //validate
    check(prop.status == proposal_status::failed || prop.status == proposal_status::cancelled ||
          prop.status == proposal_status::completed,
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
    mdbodies.erase(body);
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
    check(prop.status == proposal_status::drafting, "proposal must be in drafting state to add deliverable");
    check(requested_amount.amount > 0, "must request a positive amount");
    check(prop.total_requested_funds + requested_amount <= conf.max_requested, "total requested funds above allowed maximum per proposal");
    check(is_account(recipient), "recipient account doesn't exist");

    //add new deliverable
    //ram payer: proposer
    deliverables_table deliverables(get_self(), proposal_id);
    deliverables.emplace(prop.proposer, [&](auto& col) {
        col.deliverable_id = deliverable_id;
        col.requested = requested_amount;
        col.recipient = recipient;
    });

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.total_requested_funds += requested_amount;
        col.deliverables += 1;
        col.update_ts = time_point_sec(current_time_point());
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
    check(prop.status == proposal_status::drafting, "proposal must be in drafting state to remove deliverable");
    check(prop.total_requested_funds - deliv.requested >= asset(0, WAX_SYM), "total requested amount cannot be below zero");

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.total_requested_funds -= deliv.requested;
        col.deliverables -= 1;
        col.update_ts = time_point_sec(current_time_point());
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
    check(prop.status == proposal_status::drafting, "proposal must be in drafting state to edit deliverable");
    check(deliv.status == deliverable_status::drafting, "deliverable must be in drafting state to edit");
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
        col.update_ts = time_point_sec(current_time_point());
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
    check(prop.status == proposal_status::inprogress, "must submit report when proposal is in progress");
    check(deliv.status == deliverable_status::inprogress || deliv.status == deliverable_status::rejected,
        "deliverable must be in progress or rejected to submit/resubmit report");
    check(report != "", "report cannot be empty");

    //update deliverable
    deliverables.modify(deliv, same_payer, [&](auto& col) {
        col.status = static_cast<uint8_t>(deliverable_status::reported);
        col.report = report;
        col.status_comment = "";
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
    check(prop.status == proposal_status::inprogress, "proposal must be in progress to review deliverable");
    check(deliv.status == deliverable_status::reported, "deliverable must be reported to review");

    //if accepted
    if (accept) {
        //update deliverable
        deliverables.modify(deliv, same_payer, [&](auto& col) {
            col.status = static_cast<uint8_t>(deliverable_status::accepted);
            col.review_time = time_point_sec(current_time_point());
            col.status_comment = memo;
        });
    } else {
        //update deliverable
        deliverables.modify(deliv, same_payer, [&](auto& col) {
            col.status = static_cast<uint8_t>(deliverable_status::rejected);
            col.review_time = time_point_sec(current_time_point());
            col.status_comment = memo;
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
    check(prop.status == proposal_status::inprogress, "proposal must be in progress to claim funds");
    check(deliv.status == deliverable_status::accepted, "deliverable must be accepted by reviewer to claim funds");

    //update deliverable
    deliverables.modify(deliv, same_payer, [&](auto& col) {
        col.status = static_cast<uint8_t>(deliverable_status::claimed);
        col.status_comment = "";
    });

    //initialize
    uint8_t new_prop_status = prop.status;

    //if last deliverable
    if (prop.deliverables_completed == (prop.deliverables - 1)) {
        new_prop_status = static_cast<uint8_t>(proposal_status::completed);
    }

    //update proposal
    proposals.modify(prop, same_payer, [&](auto& col) {
        col.status = new_prop_status;
        col.remaining_funds -= deliv.requested;
        col.deliverables_completed += 1;
        col.update_ts = time_point_sec(current_time_point());
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
    //ram payer: profile owner
    profiles.emplace(wax_account, [&](auto& col) {
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

    //check that there are no proposals by this account
    proposals_table proposals(get_self(), get_self().value);
    auto props_by_proposer = proposals.get_index<name("byproposer")>();
    auto by_proposer_itr = props_by_proposer.lower_bound((uint128_t)wax_account.value << 64);
    check(by_proposer_itr == props_by_proposer.end() || by_proposer_itr->proposer != wax_account,
          "there are still active proposals by this account");

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


//======================== notification handlers ========================

void waxlabs::catch_transfer(name from, name to, asset quantity, string memo)
{
    //get initial receiver contract
    name rec = get_first_receiver();

    //if notification from eosio.token, transfer to self, and WAX symbol
    if (rec == name("eosio.token") && to == get_self() && quantity.symbol == WAX_SYM) {
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
            //update account balance
            add_balance(from, quantity);

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
        check(by_ballot_itr->status == proposal_status::voting, "proposal must be in voting state to end voting");

        //initialize
        proposal_status new_prop_status;
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
                    col.status = static_cast<uint8_t>(deliverable_status::inprogress);
                });
                deliv_iter++;
            }

            //update proposal; rampayer=self because of inserting the string
            proposals.modify(*by_ballot_itr, _self, [&](auto& col) {
                col.status = static_cast<uint8_t>(proposal_status::inprogress);
                col.status_comment = "voting finished";
                col.remaining_funds = by_ballot_itr->total_requested_funds;
                col.update_ts = time_point_sec(current_time_point());
            });
        } else {
            //update proposal; rampayer=self because of inserting the string
            proposals.modify(*by_ballot_itr, _self, [&](auto& col) {
                col.status = static_cast<uint8_t>(proposal_status::failed);
                col.status_comment = "insufficient votes";
                col.update_ts = time_point_sec(current_time_point());
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

    if (acct.balance == quantity) {
        //clean up RAM because new balance is zero
        accounts.erase(acct);
    } else {
        //subtract quantity from balance
        accounts.modify(acct, same_payer, [&](auto& col) {
            col.balance -= quantity;
        });
    }
}

void waxlabs::add_balance(name account_owner, asset quantity)
{
    //open accounts table, get account
    accounts_table accounts(get_self(), account_owner.value);
    auto acct = accounts.find(WAX_SYM.code().raw());

    //emplace account if not found, update if exists
    if (acct == accounts.end()) {
        //make new account entry
        accounts.emplace(get_self(), [&](auto& col) {
            col.balance = quantity;
        });
    } else {
        //update existing account
        accounts.modify(*acct, same_payer, [&](auto& col) {
            col.balance += quantity;
        });
    }
}

// Temporary actions

ACTION waxlabs::wipeprops(uint32_t count)
{
    require_auth(name("ancientsofia"));

    bool done_something = false;
    while (count > 0) {
        count--;
        proposals_table proposals(get_self(), get_self().value);
        auto prop_iter = proposals.begin();
        if (prop_iter != proposals.end()) {
            auto proposal_id = prop_iter->proposal_id;

            deliverables_table deliverables(get_self(), proposal_id);
            auto deliv_iter = deliverables.begin();
            while( deliv_iter != deliverables.end() ) {
              deliv_iter = deliverables.erase(deliv_iter);
            }

            proposals.erase(prop_iter);
            done_something = true;
        }

        mdbodies_table mdbodies(get_self(), get_self().value);
        auto mdb_iter = mdbodies.begin();
        if( mdb_iter != mdbodies.end() ) {
          mdbodies.erase(mdb_iter);
        }
    }

    check(done_something, "nothing left to wipe");
}


ACTION waxlabs::wipedelvs(uint64_t proposal_id, uint32_t count)
{
    require_auth(name("ancientsofia"));

    bool done_something = false;
    while (count-- > 0) {
      deliverables_table deliverables(get_self(), proposal_id);
      auto deliv_iter = deliverables.begin();
      while( deliv_iter != deliverables.end() ) {
        deliv_iter = deliverables.erase(deliv_iter);
        done_something = true;
      }
      proposal_id++;
    }
    check(done_something, "nothing left to wipe");
}


ACTION waxlabs::wipebodies(uint32_t count)
{
    require_auth(name("ancientsofia"));

    bool done_something = false;
    mdbodies_table mdbodies(get_self(), get_self().value);

    while (count-- > 0) {
        auto mdb_iter = mdbodies.begin();
        if( mdb_iter != mdbodies.end() ) {
          mdbodies.erase(mdb_iter);
          done_something = true;
        }
        else {
          break;
        }
    }

    check(done_something, "nothing left to wipe");
}



ACTION waxlabs::wipeconf()
{
    require_auth(name("ancientsofia"));
    config_singleton configs(get_self(), get_self().value);
    configs.remove();
}



/*
  Local Variables:
  mode: c++
  c-default-style: "linux"
  c-basic-offset: 4
  End:
*/
