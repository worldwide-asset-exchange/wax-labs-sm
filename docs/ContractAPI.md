# Wax Labs Contract API

## init()

Initialize the contract. Emplaces the config table.

## setversion()

Set a new contract version in the config table. Useful for tracking contract updates.

## setadmin()

Set a new admin account in the config table.

## setduration()

Sets a new vote duration, measured in seconds. This value will be used when launching a community vote to approve a proposal.

## addcategory()

Add a new proposal category to the list of approved categories.

## rmvcategory()

Remove a proposal category from the list of approved categories.

## draftprop()

Draft a new proposal.

## submitprop()

Submit a proposal draft for review by the admin account.

## reviewprop()

Approve or reject a submitted proposal. Transaction requires the authority of the designated admin account. Approved proposals will advance to the community voting phase.

## beginvoting()

Open an approved proposal for voting by the Wax community.

## endvoting()

Close out a community vote and render a final approval decision. If approved, the project begins work and can submit deliverables for review.

## setreviewer()

Set a reviewer account for a proposal that will approve deliverables.

## cancelprop()

Cancel a proposal in progress.

## deleteprop()

Delete a proposal from contract tables completely. Proposal must be in a terminal state to delete.

## newdeliv()

Add a new deliverable to a proposal. Proposal must be in the drafting phase.

## rmvdeliv()

Remove a deliverable from a proposal. Proposal must be in the drafting phase.

## editdeliv()

Edit a deliverable's requested amount and the desired recipient account. Proposal must be in the drafting phase.

## submitreport()

Submit a deliverable report for final review by the assigned proposal reviewer. Report must be accepted to claim funds for deliverable.

## claimfunds()

Claim the requested funds for a deliverable after report approval. Funds will be deposited into recipient profile account for withdrawal.

## newprofile()

Create a new Wax Labs profile with valid proposer information. Profiles are required in order to draft a proposal.

## editprofile()

Edit existing profile information.

## rmvprofile()

Remove a profile.

## withdraw()

Withdraw funds from a Wax Labs profile account.

## deleteacct()

Delete a profile account. Balance must be empty to delete. Primarily used for recovering RAM costs.

