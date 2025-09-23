//–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
// 1) POST handler: pour l'evoie du esp32 vers la base de donnee
//–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
function doPost(e) {
  // 1) Open spreadsheet and sheet
  const ss    = SpreadsheetApp
                .openById("1XwlOZeggw0O9mGbAmqE8EGU7iLcacxFPQCyRHKuDDTQ");
  const sheet = ss.getActiveSheet();

  // 2) Read header row and build header -> column index map
  const headers = sheet
    .getRange(1, 1, 1, sheet.getLastColumn())
    .getValues()[0];
    // mapping
  const colIndex = {};
  headers.forEach((h, i) => {
    colIndex[h] = i + 1;
  });

  // 3) Locate "Location" column and read existing values
  const locCol = colIndex["Location"];
  if (!locCol) throw new Error("Colonne ‘Location’ introuvable");
  const existing = sheet
    .getRange(2, locCol, sheet.getLastRow() - 1, 1)
    .getValues()
    .flat()
    .filter(String);

  // 4) Fetch incoming parameters
  const rawLoc ="1A1A1-0"  // e.g. "1A3B2-0"
  const uid    = e.parameter.uid;
  const name   = e.parameter.name;
  const now    = new Date();

  // 5) 
  //    (^\d+ : Chambre) ([A-Z] : Rangée) (\d+ : Étagère) ([A-Z] : Étage) (\d+ : Carton) - suffix
  const parts = rawLoc.match(/^(\d+)([A-Z])(\d+)([A-Z])(\d+)-(\d+)$/);
/*  if (!parts) {
    throw new Error("Format invalide pour la location : " + rawLoc);
  }*/
  let chambre   = parts[1];
  let rangee    = parts[2];
  let etagere   = parts[3];
  let niveau    = parts[4];
  let cartonNum = parseInt(parts[5], 10);
  // suffix is parts[6], but we'll regenerate

  // 6) Define valid niveau sequence
  const niveaux = ['A','B','C','D','E','F'];

  // 7) Cascade through suffix, carton, niveau to find free code
  let suffix;
  let candidate;
  suffix = 0;
  while (true) {
    candidate = `${chambre}${rangee}${etagere}${niveau}${cartonNum}-${suffix}`;
    if (!existing.includes(candidate)) {
      break; // found free
    }
    suffix++;
    if (suffix > 9) {
      suffix = 0;
      cartonNum++;
      if (cartonNum > 6) {
        cartonNum = 1;
        let idx = niveaux.indexOf(niveau);
        if (idx < niveaux.length - 1) {
          niveau = niveaux[idx + 1];
        } else {
          throw new Error("Plus de niveaux d'étagère disponibles après F");
        }
      }
    }
  }
  const finalLoc = candidate;

  // 8) Compute next empty row
  const row = sheet.getLastRow() + 1;

  // 9) Prepare data map (header names -> values)
  const data = {
    "UID":       uid,
    "Name":      name,
    "Timestamp": now,
   // "Location":  finalLoc
  };

  // 10) Write each field into correct column by name
  for (let header in data) {
    const col = colIndex[header];
    if (col) {
      sheet.getRange(row, col).setValue(data[header]);
    }
  }

  // 11) Return success
  return ContentService.createTextOutput("Success");
}


//–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
// 2) GET handler:pour l'envoir de la base de donnee vers le esp32
//–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
function doGet(e) {
  const ss    = SpreadsheetApp.openById("1XwlOZeggw0O9mGbAmqE8EGU7iLcacxFPQCyRHKuDDTQ");
  const sheet = ss.getActiveSheet();

  // Build header→column map (same as doPost)
  const headers = sheet.getRange(1,1,1,sheet.getLastColumn()).getValues()[0];
  const colIndex = {};
  headers.forEach((h,i) => colIndex[h] = i+1);

  const nameCol = colIndex["Name"];
  const locCol  = colIndex["Location"];
  if (!nameCol || !locCol) {
    return ContentService
      .createTextOutput(JSON.stringify({ error: "Missing Name or Location column" }))
      .setMimeType(ContentService.MimeType.JSON);
  }

  // Get the requested name
  const queryName = e.parameter.name;
  if (!queryName) {
    return ContentService
      .createTextOutput(JSON.stringify({ error: "No 'name' parameter provided" }))
      .setMimeType(ContentService.MimeType.JSON);
  }

  // Read all names & locations
  const lastRow    = sheet.getLastRow();
  const names      = sheet.getRange(2, nameCol, lastRow-1, 1).getValues().flat();
  const locations  = sheet.getRange(2, locCol,  lastRow-1, 1).getValues().flat();

  // Find the first row where names[i] matches queryName
  const idx = names.findIndex(n => n === queryName);
  if (idx < 0) {
    return ContentService
      .createTextOutput(JSON.stringify({ error: "Name not found" }))
      .setMimeType(ContentService.MimeType.JSON);
  }

  // Return the matching location
  const foundLocation = locations[idx];
  return ContentService
    .createTextOutput(JSON.stringify({ location: foundLocation }))
    .setMimeType(ContentService.MimeType.JSON);
}








//–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
// 1) POST handler: pour l'evoie du esp32 vers la base de donnee
//–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
function doPost(e) {
  // 1) Open spreadsheet and sheet
  const ss    = SpreadsheetApp
                .openById("1XwlOZeggw0O9mGbAmqE8EGU7iLcacxFPQCyRHKuDDTQ");
  const sheet = ss.getActiveSheet();

  // 2) Read header row and build header -> column index map
  const headers = sheet
    .getRange(1, 1, 1, sheet.getLastColumn())
    .getValues()[0];
    // mapping
  const colIndex = {};
  headers.forEach((h, i) => {
    colIndex[h] = i + 1;
  });

  // 4) Fetch incoming parameters
  const uid    = e.parameter.uid;
  const name   = e.parameter.name;
  const now    = new Date();
  

  // 8) Compute next empty row
  const row = sheet.getLastRow() + 1;

  // 9) Prepare data map (header names -> values)
  const data = {
    "UID":       uid,
    "Name":      name,
    "Timestamp": now,
   // "Location":  finalLoc
  };
  
  // 10) Write each field into correct column by name
  for (let header in data) {
    const col = colIndex[header];
    if (col) {
      sheet.getRange(row, col).setValue(data[header]);
    }
  }
  // 11) Return success
  return ContentService.createTextOutput("Success");
}