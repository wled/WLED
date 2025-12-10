function drawBoxes(inputPixelArray, widthPixels, heightPixels) {
 
    var w = window;

    // Get the canvas context
    var ctx = canvas.getContext('2d', { willReadFrequently: true });

    // Set the width and height of the canvas
    if (w.innerHeight < w.innerWidth) {
        canvas.width = Math.floor(w.innerHeight * 0.98);
    }
    else{
        canvas.width = Math.floor(w.innerWidth * 0.98);
    }
    //canvas.height = w.innerWidth;

    let pixelSize = Math.floor(canvas.width/widthPixels);

    let xOffset = (w.innerWidth - (widthPixels * pixelSize))/2

    //Set the canvas height to fit the right number of pixelrows
    canvas.height = (pixelSize * heightPixels) + 10
    
    //Iterate through the matrix
    for (let y = 0; y < heightPixels; y++) {
        for (let x = 0; x < widthPixels; x++) {

            // Calculate the index of the current pixel
            let i = (y*widthPixels) + x;
            
            //Gets the RGB of the current pixel
            let pixel = inputPixelArray[i];

            let pixelColor = 'rgb(' + pixel[0] + ', ' + pixel[1] + ', ' + pixel[2] + ')';

            let textColor = 'rgb(128,128,128)';

            // Set the fill style to the pixel color
            ctx.fillStyle = pixelColor;

            //Draw the rectangle
            ctx.fillRect(x * pixelSize, y * pixelSize, pixelSize, pixelSize);

            // Draw a border on the box
            ctx.strokeStyle = '#888888';
            ctx.lineWidth = 1;
            ctx.strokeRect(x * pixelSize, y * pixelSize, pixelSize, pixelSize);

            //Write text to box
            ctx.font = "10px Arial";
            ctx.fillStyle = textColor;
            ctx.textAlign = "center";
            ctx.textBaseline = 'middle';
            ctx.fillText((pixel[4] + 1), (x * pixelSize) + (pixelSize /2), (y * pixelSize) + (pixelSize /2));
        }
    }
    var imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    canvas.width = w.innerWidth;
    ctx.putImageData(imageData, xOffset, 0);
}

