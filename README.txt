=================
Remove Watermarks
=================

In the context of this project, i have implemented
a method for removing graphic elements from an image.
The elements to be removed are intended to mark
copyright ownership of an image and are known in
English as 'watermarks'.


The recommended method for removing watermarks involves
utilizing the content boundaries from both images
the one containing the watermark and the one without the watermark.

Detection and storage of boundaries in an image:
Transforming the pixel colors of the input image from the (r, g, b) color
model to grayscale.
Calculating the result of the convolution operation for each pixel using
the Sobel filter convolution masks. A combination of the results obtained
for each convolution mask is used.
Binarizing the resulting output for each pixel based on a threshold.

After the process described above, matches for the binarized shape of the
watermark are sought in windows of the same resolution overlaid on the
binarized shape of the image containing occurrences of the watermark.
A match is considered when, after the described overlay, a very high
proportion of cells with a value of 1 in the binarized shape of the
watermark also have a value of 1 in the overlaid window. Cells with a
value of 0 in the binarized shape of the watermark are not considered
in the matching process.