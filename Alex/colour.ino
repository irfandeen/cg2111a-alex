// Colour Sensor Driver & k-NN Classifier Code

#define MAX_PARTITION 300 // reduce partition for faster classification
#define S0 36             // purple wire
#define S1 34             // green wire
#define S2 32             // yellow wire
#define S3 30             // grey wire
#define sensorOut 43      // blue wire

/* USAGE
 *  call colourSetup() within Arduino main setup()
 *
 *  When detecting colour,
 *  call getColour() returns TColour value (refer to constants.h)
 */

void colourSetup() {
    pinMode(S0, OUTPUT);
    pinMode(S1, OUTPUT);
    pinMode(S2, OUTPUT);
    pinMode(S3, OUTPUT);

    // Setting the sensorOut as an input
    pinMode(sensorOut, INPUT);

    // Setting frequency scaling to 20%
    digitalWrite(S0, HIGH);
    digitalWrite(S1, LOW);
}

// getRGB will interface with TCS3200 colour sensor and return raw RGB frequencies
RGB getRGB() {
    int redFrequency = 0;
    int greenFrequency = 0;
    int blueFrequency = 0;

    digitalWrite(S2, LOW);
    digitalWrite(S3, LOW);
    redFrequency = pulseIn(sensorOut, LOW);
    delay(100);

    digitalWrite(S2, HIGH);
    digitalWrite(S3, HIGH);
    greenFrequency = pulseIn(sensorOut, LOW);
    delay(100);

    digitalWrite(S2, LOW);
    digitalWrite(S3, HIGH);
    blueFrequency = pulseIn(sensorOut, LOW);
    delay(100);

    // Return RGB for classification
    RGB newCol = {redFrequency, greenFrequency, blueFrequency};
    return newCol;
}

unsigned long sqr(int a) {
    return a * a;
}

unsigned long euclidError(RGB col1, RGB col2) {
    unsigned long totalError = sqr(col1.red - col2.red) + sqr(col1.green - col2.green) + sqr(col1.blue - col2.blue);
    return totalError;
}

void sort(ErrorData errors[], int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n - i; j++) {
            if (errors[j].error > errors[j + 1].error) {
                ErrorData tmp = {errors[j].error, errors[j].colClass};
                errors[j] = {errors[j + 1].error, errors[j + 1].colClass};
                errors[j + 1] = {tmp.error, tmp.colClass};
            }
        }
    }
}

TColour kNNClassifier(RGB col, int dataset[MAX_PARTITION][MAX_PARAM]) {
    // Empirically Determined k-Value
    const int k = 5;

    // Calculate Euclidean Error between all data points
    TColour predictedCol = RED;
    unsigned long minDist = 1e9;
    ErrorData errors[MAX_PARTITION];
    for (int i = 0; i < MAX_PARTITION; i++) {
        RGB col2 = {dataset[i][0], dataset[i][1], dataset[i][2]};
        unsigned long error = euclidError(col, col2);
        errors[i] = {euclidError(col, col2), dataset[i][3]};
    }

    sort(errors, MAX_PARTITION);

    // Populating histogram of k nearest colours
    int tiebreaker[3] = {0, 0, 0};
    int votes = 0;
    for (long i = 0; i < k; i++) {
        tiebreaker[errors[i].colClass]++;
    }

    // Finding the most common colour
    for (int i = RED; i <= WHITE; i++) {
        if (tiebreaker[i] > votes) {
            votes = tiebreaker[i];
            predictedCol = (TColour)i;
        }
    }

    return predictedCol;
}

TColour getColour() {
    RGB col = getRGB();
    return kNNClassifier(col, data);
}
