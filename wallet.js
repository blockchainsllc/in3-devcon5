const fs = require('fs')
const IN3 = require('in3').default

const erc20Abi = [
    {
        "constant": true,
        "inputs": [],
        "name": "decimals",
        "outputs": [{"name": "","type": "uint8" }],
        "payable": false,
        "stateMutability": "view",
        "type": "function"
    },
    {
        "constant": true,
        "inputs": [{"name": "_owner","type": "address"}],
        "name": "balanceOf",
        "outputs": [{"name": "balance", "type": "uint256"}],
        "payable": false,
        "stateMutability": "view",
        "type": "function"
    },
    {
        "constant": true,
        "inputs": [],
        "name": "symbol",
        "outputs": [{"name": "","type": "string"}],
        "payable": false,
        "stateMutability": "view",
        "type": "function"
    },
    {
        "anonymous": false,
        "inputs": [
            {"indexed": true,"name": "from","type": "address"},
            {"indexed": true, "name": "to", "type": "address"},
            {"indexed": false,"name": "value","type": "uint256"}
        ],
        "name": "Transfer",
        "type": "event"
    }
]

// file where to store the account-data
const accountsFile = './accounts.json'

const in3 = new IN3({
    chainId: 'mainnet',
    cacheStorage: {
        setItem(key, value) {
            fs.writeFileSync(key, value, 'utf8')
        },
        getItem(key) {
            try {
                return fs.readFileSync(key, 'utf8')
            } catch (ex) {
                return null
            }
        }
    }
})

function readAccounts() {
    try {
        return JSON.parse(fs.readFileSync(accountsFile, 'utf8'))
    }
    catch (ex) {
        return []
    }
}

function addAccount(name, address) {
    const accounts = readAccounts()
    accounts.push({ name, address })
    fs.writeFileSync(accountsFile, JSON.stringify(accounts, null, 2), 'utf8')
}

async function showLogs(token) {
    const tokenContract = in3.eth.contractAt(erc20Abi, token)
    const [blockNumber, decimals, sym] = await Promise.all([
        in3.eth.blockNumber(),
        tokenContract.decimals(),
        tokenContract.symbol()
    ])
    const logs = await tokenContract.events.Transfer.getLogs({ fromBlock: blockNumber - 50 })
    for (const l of logs)
        console.log(`${parseInt(l.log.blockNumber)} : Transfer ${l.event.from} -> ${l.event.to} : ${l.event.value.toNumber() / Math.pow(10, decimals)} ${sym}`)
}

async function showBalance(token) {
    const tokenContract = in3.eth.contractAt(erc20Abi, token)

    const unit = token
        ? await in3.eth.callFn(token, 'symbol():(string)')
        : 'wei'

    for (let account of readAccounts()) {
        const balance = token
            ? await in3.eth.callFn(token, 'balanceOf(address):(uint256)', account.address)
            : await in3.eth.getBalance(account.address)

        console.log(`${account.name} : ${balance.toString()} ${unit}`)
    }
}

async function showBlockNumber() {
    const bn = await in3.eth.blockNumber()
    console.log("current Block : ", bn)
}

async function main() {
    const cmd = process.argv[2]
    switch (cmd) {
        case 'block':
            return await showBlockNumber()
        case 'add':
            return addAccount(process.argv[3], process.argv[4])
        case 'balance':
            return await showBalance(process.argv[3])
        case 'logs':
            return await showLogs(process.argv[3])
    }
}

main().catch(console.error)



