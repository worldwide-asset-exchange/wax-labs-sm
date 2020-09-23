# WAX Labs
A funding platform for WAX Labs proposals.

## Dependencies

* eosio.cdt
* nodeos, cleos, keosd

## Setup

To begin, navigate to the project directory: `wax-labs-contracts/`

    mkdir build && mkdir build/labs

    chmod +x build.sh && chmod +x deploy.sh

The `labs` contract has already been implemented and is build-ready.

## Build

    ./build.sh labs

## Deploy

    ./deploy.sh labs labs.decide { mainnet | testnet | local }

# Documentation

### [User Guide](docs/UserGuide.md)

### [WAX Labs Contract API](docs/ContractAPI.md)
