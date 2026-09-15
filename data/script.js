let cardData = [];
let users = [];
let formatsLoaded = false;
const tableBody = document.getElementById('cardTable').getElementsByTagName('tbody')[0];
const userTableBody = document.getElementById('userTable').getElementsByTagName('tbody')[0];
const lastReadCardsTableBody = document.getElementById('lastReadCardsTable').getElementsByTagName('tbody')[0];
const formatTableBody = document.getElementById('formatTable').getElementsByTagName('tbody')[0];
const importExportArea = document.getElementById('importExportArea');
const currentCardPanel = document.getElementById('currentCard');

function setText(cell, value) {
    cell.textContent = value === undefined || value === null ? '' : value;
}

function cardLabel(card) {
    if (!card || !card.bitCount) {
        return '';
    }
    return `FC ${card.facilityCode} / CN ${card.cardNumber}`;
}

function formatLabel(card) {
    if (!card) {
        return '';
    }
    if (card.formatDescription) {
        return card.formatDescription;
    }
    if (card.formatCandidateCount > 0) {
        return `${card.formatViableCount}/${card.formatCandidateCount} candidates`;
    }
    return 'Unsupported';
}

function statusClass(status) {
    if (status === 'Authorized') {
        return 'authorized';
    }
    if (status === 'Unauthorized') {
        return 'unauthorized';
    }
    if (status === 'Ambiguous' || status === 'No format match') {
        return 'diagnostic';
    }
    return '';
}

function canAuthorize(card) {
    return card &&
        card.status === 'Unauthorized' &&
        card.formatId &&
        card.formatCandidateCount === 1 &&
        card.formatViableCount === 1;
}

function userExists(card) {
    return users.some(user =>
        Number(user.facilityCode) === Number(card.facilityCode) &&
        Number(user.cardNumber) === Number(card.cardNumber)
    );
}

function updateCurrentCard(card) {
    currentCardPanel.className = 'card-panel';
    if (!card) {
        currentCardPanel.classList.add('empty');
        currentCardPanel.innerHTML = '<h2>No card read yet</h2><p>Present a card to populate this dashboard.</p>';
        return;
    }

    const cssClass = statusClass(card.status);
    if (cssClass) {
        currentCardPanel.classList.add(cssClass);
    }

    const safeHex = card.hexCardData || '';
    const safeRaw = card.rawCardData || '';
    currentCardPanel.innerHTML = `
        <h2>${card.status || 'Read'} — ${card.bitCount || 0} bits</h2>
        <div class="dashboard-grid">
            <div><strong>Format</strong><span>${formatLabel(card)}</span></div>
            <div><strong>Facility Code</strong><span>${card.facilityCode || 0}</span></div>
            <div><strong>Card Number</strong><span>${card.cardNumber || 0}</span></div>
            <div><strong>Authorization</strong><span>${authorizationMessage(card)}</span></div>
        </div>
        <p><strong>Details:</strong> ${card.details || ''}</p>
        <p><strong>Hex:</strong> <button onclick="copyToClipboard('${safeHex}')">Copy</button> <code>${safeHex}</code></p>
        <p><strong>Raw:</strong> <button onclick="copyToClipboard('${safeRaw}')">Copy</button> <code class="raw-data">${safeRaw}</code></p>
    `;
}

function authorizationMessage(card) {
    if (card.status === 'Authorized') {
        return 'Allowed by authorized-card list';
    }
    if (card.status === 'Unauthorized') {
        return card.formatCandidateCount === 1 ? 'Decoded but not in authorized-card list' : 'Blocked';
    }
    if (card.status === 'Ambiguous') {
        return 'Blocked: ambiguous format';
    }
    if (card.status === 'No format match') {
        return 'Blocked: no viable format';
    }
    return 'Not eligible';
}

function updateTable() {
    fetch('/getCards')
        .then(response => response.json())
        .then(data => {
            cardData = data;
            tableBody.innerHTML = '';
            updateCurrentCard(data.length ? data[data.length - 1] : null);
            data.forEach((card, index) => {
                let row = tableBody.insertRow();
                const cssClass = statusClass(card.status);
                if (cssClass) {
                    row.classList.add(cssClass);
                }
                let cellIndex = row.insertCell(0);
                let cellStatus = row.insertCell(1);
                let cellBitLength = row.insertCell(2);
                let cellFormat = row.insertCell(3);
                let cellFacilityCode = row.insertCell(4);
                let cellCardNumber = row.insertCell(5);
                let cellHexData = row.insertCell(6);
                let cellRawData = row.insertCell(7);

                setText(cellIndex, index + 1);
                setText(cellStatus, card.status);
                setText(cellBitLength, card.bitCount);
                setText(cellFormat, formatLabel(card));
                setText(cellFacilityCode, card.facilityCode);
                setText(cellCardNumber, card.cardNumber);
                cellHexData.innerHTML = `<button onclick="copyToClipboard('${card.hexCardData || ''}')">Copy</button> <code>${card.hexCardData || ''}</code>`;
                cellRawData.innerHTML = `<code class="raw-data">${card.rawCardData || ''}</code>`;
            });
        })
        .catch(error => console.error('Error fetching card data:', error));
}

function updateUserTable() {
    fetch('/getUsers')
        .then(response => response.json())
        .then(data => {
            users = data;
            userTableBody.innerHTML = '';
            data.forEach((user, index) => {
                let row = userTableBody.insertRow();
                let cellIndex = row.insertCell(0);
                let cellFacilityCode = row.insertCell(1);
                let cellCardNumber = row.insertCell(2);
                let cellName = row.insertCell(3);
                let cellAction = row.insertCell(4);

                setText(cellIndex, index + 1);
                setText(cellFacilityCode, user.facilityCode);
                setText(cellCardNumber, user.cardNumber);
                setText(cellName, user.name);
                cellAction.innerHTML = '<button onclick="deleteCard(' + index + ')">Delete</button>';
            });

            let inputRow = userTableBody.insertRow();
            inputRow.className = 'inputRow';
            inputRow.insertCell(0);
            let cellFacilityCode = inputRow.insertCell(1);
            let cellCardNumber = inputRow.insertCell(2);
            let cellName = inputRow.insertCell(3);
            let cellAction = inputRow.insertCell(4);

            cellFacilityCode.innerHTML = '<input type="number" id="newFacilityCode">';
            cellCardNumber.innerHTML = '<input type="number" id="newCardNumber">';
            cellName.innerHTML = '<input type="text" id="newName" maxlength="49">';
            cellAction.innerHTML = '<button onclick="addCard()">Save</button>';
            updateLastReadCardsTable();
        })
        .catch(error => console.error('Error fetching user data:', error));
}

function updateLastReadCardsTable() {
    fetch('/getCards')
        .then(response => response.json())
        .then(data => {
            cardData = data;
            lastReadCardsTableBody.innerHTML = '';
            const last10Cards = data.slice(-10).reverse();
            last10Cards.forEach((card, index) => {
                let row = lastReadCardsTableBody.insertRow();
                const cssClass = statusClass(card.status);
                if (cssClass) {
                    row.classList.add(cssClass);
                }
                let cellIndex = row.insertCell(0);
                let cellStatus = row.insertCell(1);
                let cellCard = row.insertCell(2);
                let cellFormat = row.insertCell(3);
                let cellDetails = row.insertCell(4);
                let cellAction = row.insertCell(5);

                setText(cellIndex, data.length - index);
                setText(cellStatus, card.status);
                setText(cellCard, cardLabel(card));
                setText(cellFormat, formatLabel(card));
                setText(cellDetails, card.details);
                if (canAuthorize(card) && !userExists(card)) {
                    cellAction.innerHTML = `<button onclick="authorizeCardFromRead(${data.length - 1 - index})">Authorize</button>`;
                } else if (userExists(card)) {
                    setText(cellAction, 'Already authorized');
                } else {
                    setText(cellAction, 'Not eligible');
                }
            });

            for (let i = last10Cards.length; i < 10; i++) {
                let row = lastReadCardsTableBody.insertRow();
                for (let c = 0; c < 6; c++) {
                    setText(row.insertCell(c), c === 0 ? i + 1 : '');
                }
            }
        })
        .catch(error => console.error('Error fetching last read card data:', error));
}

function addCard() {
    const facilityCode = document.getElementById('newFacilityCode').value;
    const cardNumber = document.getElementById('newCardNumber').value;
    const name = document.getElementById('newName').value;
    addCardValues(facilityCode, cardNumber, name);
}

function addCardValues(facilityCode, cardNumber, name) {
    const params = new URLSearchParams({
        facilityCode: facilityCode,
        cardNumber: cardNumber,
        name: name
    });

    fetch(`/addCard?${params.toString()}`)
        .then(response => {
            if (response.ok) {
                updateUserTable();
                updateLastReadCardsTable();
                alert('Card added successfully');
            } else {
                response.text().then(text => alert(text || 'Failed to add card'));
            }
        })
        .catch(error => console.error('Error adding card:', error));
}

function authorizeCardFromRead(index) {
    const card = cardData[index];
    if (!canAuthorize(card)) {
        alert('Only uniquely decoded unauthorized cards can be authorized.');
        return;
    }

    const defaultName = card.formatDescription ? `${card.formatDescription} ${card.cardNumber}` : `Card ${card.cardNumber}`;
    const name = prompt('Name for this authorized card:', defaultName);
    if (name === null) {
        return;
    }
    addCardValues(card.facilityCode, card.cardNumber, name.trim() || defaultName);
}

function deleteCard(index) {
    fetch(`/deleteCard?index=${index}`)
        .then(response => {
            if (response.ok) {
                updateUserTable();
                alert('Card deleted successfully');
            } else {
                alert('Failed to delete card');
            }
        })
        .catch(error => console.error('Error deleting card:', error));
}

function showSection(section) {
    document.getElementById('lastRead').classList.add('hidden');
    document.getElementById('ctfMode').classList.add('hidden');
    document.getElementById('formats').classList.add('hidden');
    document.getElementById('settings').classList.add('hidden');
    document.getElementById(section).classList.remove('hidden');
    if (section === 'formats' && !formatsLoaded) {
        updateFormatTable();
    }
}

function toggleCollapsible() {
    const content = document.querySelector('.contentCollapsible');
    content.style.display = content.style.display === 'block' ? 'none' : 'block';
}

function toggleWelcomeMessage() {
    const welcomeMessage = document.getElementById('welcomeMessage').value;
    const customMessageDiv = document.getElementById('customMessageDiv');
    if (welcomeMessage === 'custom') {
        customMessageDiv.removeAttribute('hidden');
    } else {
        customMessageDiv.setAttribute('hidden', 'hidden');
    }
}

function updateSettingsUI(settings) {
    document.getElementById('modeSelect').value = settings.mode;
    document.getElementById('timeoutSelect').value = settings.displayTimeout;
    document.getElementById('ap_ssid').value = settings.apSsid;
    document.getElementById('ap_passphrase').value = settings.apPassphrase;
    document.getElementById('ssid_hidden').checked = settings.ssidHidden;
    document.getElementById('ap_channel').value = settings.apChannel;
    document.getElementById('welcomeMessage').value = settings.welcomeMessage;
    document.getElementById('customMessage').value = settings.customMessage;
    document.getElementById('ledValid').value = settings.ledValid;
    document.getElementById('spkOnValid').value = settings.spkOnValid;
    document.getElementById('spkOnInvalid').value = settings.spkOnInvalid;
    toggleWelcomeMessage();
}

function saveSettings() {
    const settings = {
        mode: document.getElementById('modeSelect').value,
        displayTimeout: parseInt(document.getElementById('timeoutSelect').value, 10),
        apSsid: document.getElementById('ap_ssid').value,
        apPassphrase: document.getElementById('ap_passphrase').value,
        ssidHidden: document.getElementById('ssid_hidden').checked,
        apChannel: parseInt(document.getElementById('ap_channel').value),
        welcomeMessage: document.getElementById('welcomeMessage').value,
        customMessage: document.getElementById('customMessage').value,
        ledValid: parseInt(document.getElementById('ledValid').value),
        spkOnValid: parseInt(document.getElementById('spkOnValid').value),
        spkOnInvalid: parseInt(document.getElementById('spkOnInvalid').value)
    };

    fetch('/saveSettings', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(settings)
    })
        .then(response => {
            if (response.ok) {
                alert('Settings saved successfully');
            } else {
                alert('Failed to save settings');
            }
        })
        .catch(error => console.error('Error saving settings:', error));
}

function fetchSettings() {
    fetch('/getSettings')
        .then(response => response.json())
        .then(data => {
            updateSettingsUI(data);
        })
        .catch(error => console.error('Error fetching settings:', error));
}

function updateFormatTable() {
    fetch('/getWiegandFormats')
        .then(response => response.json())
        .then(data => {
            formatsLoaded = true;
            formatTableBody.innerHTML = '';
            data.forEach(format => {
                let row = formatTableBody.insertRow();
                setText(row.insertCell(0), format.id);
                setText(row.insertCell(1), format.description);
                setText(row.insertCell(2), format.bitCount);
                setText(row.insertCell(3), `${format.facilityCodeStart}-${format.facilityCodeEnd}`);
                setText(row.insertCell(4), `${format.cardNumberStart}-${format.cardNumberEnd}`);
                setText(row.insertCell(5), format.hasParity ? `${format.parityRuleCount || 2} rule(s)` : 'None');
            });
        })
        .catch(error => console.error('Error fetching Wiegand formats:', error));
}

function exportData() {
    fetch('/getUsers')
        .then(response => response.json())
        .then(data => {
            const dataString = JSON.stringify(data);
            importExportArea.value = dataString;
            importExportArea.select();
            document.execCommand('copy');
            alert('Data exported to clipboard');
        })
        .catch(error => console.error('Error exporting data:', error));
}

function importData() {
    const dataString = importExportArea.value;
    try {
        const data = JSON.parse(dataString);
        if (Array.isArray(data)) {
            data.forEach(card => {
                addCardValues(card.facilityCode, card.cardNumber, card.name);
            });
            updateUserTable();
            alert('Data imported successfully');
        } else {
            alert('Invalid data format');
        }
    } catch (error) {
        alert('Invalid JSON format');
    }
}

function copyToClipboard(text) {
    const tempInput = document.createElement('input');
    tempInput.style.position = 'absolute';
    tempInput.style.left = '-9999px';
    tempInput.value = text;
    document.body.appendChild(tempInput);
    tempInput.select();
    document.execCommand('copy');
    document.body.removeChild(tempInput);
}

window.onload = fetchSettings;
setInterval(updateTable, 5000);
setInterval(updateLastReadCardsTable, 5000);
updateTable();
updateUserTable();
updateLastReadCardsTable();
