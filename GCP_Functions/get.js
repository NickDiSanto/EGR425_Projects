/**
 * Responds to any HTTP request.
 *
 * @param {!express:Request} req HTTP request context.
 * @param {!express:Response} res HTTP response context.
 */

const functions = require('@google-cloud/functions-framework');
const {Firestore} = require('@google-cloud/firestore');
const {Storage} = require('@google-cloud/storage');
const fs = require("fs"); // File System

const gcsBucketName = "egr425-2023";
const gfsCollectionName = "egr425-2023";

exports.egr425_get = async (req, res) => {

  const strHeaderM5 = req.get('M5-Details');
	console.log(`M5-Details: ${strHeaderM5}`);

  let objDetails;
	try {
		// Converting String to JSON object
		objDetails = JSON.parse(strHeaderM5);
		console.log("objDetails:" + objDetails);

		// Check that JSON object has essential components
		if (!objDetails.hasOwnProperty("userId") || !objDetails.hasOwnProperty("timeDuration") ||
			!objDetails.hasOwnProperty("dataType")) {
				let message = 'Could NOT parse JSON details in header';
				console.error(message);
				res.status(400).send("Malformed request.");
			}
	} catch(e) {
		let message = 'Could NOT parse JSON details from header.';
		console.error(message);
		res.status(400).send("Malformed request.");
	}

  const userId = objDetails.userId;
  const timeDuration = objDetails.timeDuration;
  const dataType = objDetails.dataType;

  const reqSum = `User: ${userId}, dur: ${timeDuration}, dtype: ${dataType}%`;
  console.log(reqSum);

  const firestore = new Firestore();
  const storage = new Storage();


  let message = ''
  if(userId=="all"){
	  try {
		let count=0;
		let total=0;
		let units="";
		let max=0;
		let min=Date.now();
		await firestore.collection(gfsCollectionName)
		.where("otherDetails.cloudUploadTime", ">", Date.now() - (timeDuration*1000))
		.get()
		.then(querySnapshot => {
			querySnapshot.forEach(doc => {
			const data=doc.data()
			count+=1;
			if(dataType=="Temp"){
				total+=data.shtDetails.temp;
				units=" Celsius";
			}else if(dataType=="rHum"){
				total+=data.shtDetails.rHum;
				units="%";
			}else if(dataType=="lux"){
				total+=data.vnlDetails.al;
				units=" lux";
			}
			if(data.otherDetails.cloudUploadTime>max){
				max=data.otherDetails.cloudUploadTime;
			}
			if(data.otherDetails.cloudUploadTime<min){
				min=data.otherDetails.cloudUploadTime;
			}
			
		})});
		avg=total/count;
		minDate=new Date(min);
		maxDate=new Date(max);

		message =`avg: ${avg.toFixed(1)}${units}, from ${minDate.toString().substring(0,21)} to${maxDate.toString().substring(0,21)}, over ${count} datapoints, rate: every ${((max-min)/1000)/count} secs`;
		console.log(message);
		console.log(total);
		res.status(200).send(message);
	} catch (e) {
		let eMessage = `failed to get docs for ${gfsCollectionName}: ${e}`;
		console.log(eMessage)
	}
  }else{
	  try {
		let count=0;
		let total=0;
		let units="";
		let max=0;
		let min=Date.now();
		await firestore.collection(gfsCollectionName)
		.doc("users")
		.collection(userId)
		.where("otherDetails.cloudUploadTime", ">", Date.now() - (timeDuration*1000))
		.get()
		.then(querySnapshot => {
			querySnapshot.forEach(doc => {
			const data=doc.data()
			count+=1;
			if(dataType=="Temp"){
				total+=data.shtDetails.temp;
				units=" Celsius";
			}else if(dataType=="rHum"){
				total+=data.shtDetails.rHum;
				units="%";
			}else if(dataType=="lux"){
				total+=data.vnlDetails.al;
				units=" lux";
			}
			if(data.otherDetails.cloudUploadTime>max){
				max=data.otherDetails.cloudUploadTime;
			}
			if(data.otherDetails.cloudUploadTime<min){
				min=data.otherDetails.cloudUploadTime;
			}
			
		})});
		avg=total/count;
		minDate=new Date(min);
		maxDate=new Date(max);

		message = `avg: ${avg.toFixed(1)}${units}, from ${minDate.toString().substring(0,21)} to${maxDate.toString().substring(0,21)}, over ${count} datapoints, rate: every ${((max-min)/1000)/count} secs`;
		console.log(message);
		console.log(total);
		res.status(200).send(message);
	} catch (e) {
		let eMessage = `failed to get docs for ${gfsCollectionName}: ${e}`;
		console.log(eMessage)
	}
  }
  
  res.status(400).send(message);
};
