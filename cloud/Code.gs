/** Smart Factory classroom endpoint: use with NON-SENSITIVE demo data only.
 * Script Properties: SPREADSHEET_ID, DEVICE_TOKEN
 * Deploy: Web app, Execute as Me, Anyone (demo only).
 * The token authenticates ingestion; a public dashboard is visible to visitors with the URL.
 */
const COLUMNS=['received_at','message_id','machine_id','temperature','humidity','vibration','energy_kw','machine_status','total_count','good_count','defect_count'];
function sheet_(){
  const id=PropertiesService.getScriptProperties().getProperty('SPREADSHEET_ID');
  if(!id) throw Error('Set SPREADSHEET_ID in Script Properties');
  const ss=SpreadsheetApp.openById(id);
  let sh=ss.getSheetByName('telemetry');
  if(!sh) sh=ss.insertSheet('telemetry');
  if(sh.getLastRow()===0) sh.appendRow(COLUMNS);
  return sh;
}
function setup(){sheet_();return 'telemetry ready';}
function reply_(obj){return ContentService.createTextOutput(JSON.stringify(obj)).setMimeType(ContentService.MimeType.JSON);}
function doPost(e){
  try{
    const payload=JSON.parse(e.postData.contents||'{}');
    const expected=PropertiesService.getScriptProperties().getProperty('DEVICE_TOKEN');
    if(!expected || String(payload.device_token||'')!==expected) return reply_({ok:false,error:'unauthorized'});
    const id=String(payload.machine_id||'');
    if(!['M01','M02'].includes(id)) return reply_({ok:false,error:'invalid machine_id'});
    const mid=String(payload.message_id||'');
    if(!/^[A-Za-z0-9._:-]{3,100}$/.test(mid))return reply_({ok:false,error:'invalid message_id'});
    const numeric=['temperature','humidity','vibration','energy_kw','total_count','good_count','defect_count'];
    if(!numeric.every(k=>typeof payload[k]==='number'&&Number.isFinite(payload[k]))) return reply_({ok:false,error:'invalid numeric fields'});
    if(payload.total_count<0||payload.good_count<0||payload.defect_count<0||payload.good_count+payload.defect_count>payload.total_count)return reply_({ok:false,error:'invalid counters'});
    const status=String(payload.machine_status||'');
    if(!['RUNNING','WARNING'].includes(status))return reply_({ok:false,error:'invalid status'});
    const lock=LockService.getScriptLock();
    lock.waitLock(10000);
    try{
      const sh=sheet_();
      const n=sh.getLastRow();
      if(n>1){
        const ids=sh.getRange(2,2,n-1,1).createTextFinder(mid).matchEntireCell(true).findNext();
        if(ids)return reply_({ok:true,duplicate:true,message_id:mid});
      }
      sh.appendRow([new Date(),mid,id,...numeric.slice(0,4).map(k=>payload[k]),status,...numeric.slice(4).map(k=>payload[k])]);
      return reply_({ok:true,stored:true,message_id:mid,machine_id:id});
    }finally{lock.releaseLock();}
  }catch(err){return reply_({ok:false,error:String(err.message||err)});}
}
function doGet(e){
  if(e && e.parameter && e.parameter.view==='dashboard')return HtmlService.createHtmlOutputFromFile('Dashboard').setTitle('Smart Factory BI');
  return reply_({ok:true,service:'SmartFactory IoT',hint:'Use ?view=dashboard for dashboard; POST telemetry with device_token'});
}
function getDashboardData(){
  // Used only within Apps Script HTML Service through google.script.run.
  const values=sheet_().getDataRange().getValues();
  return values.slice(1).slice(-2000).map(r=>({
    timestamp:r[0] instanceof Date?r[0].toISOString():String(r[0]),
    message_id:String(r[1]),machine_id:String(r[2]),temperature:Number(r[3]),humidity:Number(r[4]),
    vibration:Number(r[5]),energy_kw:Number(r[6]),machine_status:String(r[7]),
    total_count:Number(r[8]),good_count:Number(r[9]),defect_count:Number(r[10])
  }));
}
