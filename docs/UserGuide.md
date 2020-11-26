# Get Started

## Initial Setup

Pre: Ensure the Decide platform is in production and the WAX Labs account has registered as a voter to the core treasury.

1. Initial contract setup begins by calling the init() action. This created the config table and readies the contract for usage.

2. Top up the Labs fund by transferring WAX to the contract with the memo set as "fund".

3. Deposit funds as a user by transferring WAX to the contract from the user account. This will create a WAX Labs internal deposit that is used to pay for all fees, and where awarded funds will be deposited. Tokens can be withdrawn at any time.

### Tip: To skip notification logic use the "skip" memo - this is useful for managing contrct resources without mixing tokens with the Wax Labs accounting.
