#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
const uint16_t postLockSample = 3;
const uint16_t dataLength = 10000;
const float vel_water = 1480;
const float pulseWidth = 4.0;
 

//////////////////////////////////////////////////////////////////////////////
float argmax(const float* arr, uint16_t size)
{   
    float tempMax = 0;
    for(size_t i = 0; i< size; i++)
    {
        if(*(arr+i) >= tempMax)
            tempMax = *(arr+i); 
    }

    return tempMax;
}


//////////////////////////////////////////////////////////////////////////////
uint16_t argFirstLarger(const float *arr, uint16_t start, uint16_t end, float threshold){

    uint16_t maxInd = start;

    for(size_t i=start; i<end; i++){
        if((*(arr+i)) >= threshold)
        {
            maxInd = i;
            for(size_t j = i+1; j < i+postLockSample; j++)
            {   
                if(j >= dataLength)
                    return 0;

                if(*(arr+j) < threshold)
                {
                    maxInd = j;
                    return argFirstLarger(arr, maxInd, dataLength, threshold);
                }
            }
            return maxInd;
        }
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
uint16_t argFirstSmaller(const float *arr, uint16_t start, uint16_t end, float threshold){


    uint16_t minInd = start;

    for(size_t i=start; i<end; i++){
        if((*(arr+i)) <= threshold)
        {
            minInd = i;
            for(size_t j = i+1; j < i+postLockSample; j++)
            {   
                if(j >= dataLength)
                    return 0;

                if(*(arr+j) > threshold)
                {
                    minInd = j;
                    return argFirstSmaller(arr, minInd, dataLength, threshold);
                }
            }
            return minInd;
        }

    }
    return 0;

}


// @brief: Function calculating the thickness based on inputs of velocity, sampling rate, pipe inner diameter, norminal thickness and threshold to try in (0-1]
float thickCal(const float *arr,  float velocity, float fs, float id, float targetThick, float _thres)
{

    // 0. preparation calculation
    float maxValue = argmax(arr, dataLength / 2); // we know for certain the peak value is within the first half.
    float threshold = _thres * maxValue;

    uint16_t waterGap = id/1000 / vel_water * fs;
    uint16_t targetThickDataPoint = targetThick/1000 * 2 / velocity * fs;


    // 1. front wall with waterGap data points margin in the beginning
    // uint16_t _front = argFirstLarger(arr, waterGap, dataLength, threshold);  
   
    uint16_t gateA_f = argFirstLarger(arr, waterGap, dataLength, threshold);
    if(gateA_f == 0)
        return -1;

    uint16_t gateA_b = argFirstSmaller(arr, gateA_f, gateA_f + targetThickDataPoint, threshold);
    if(gateA_b == 0)
        return -2;

    uint16_t gateA = (gateA_f + gateA_b) / 2;


    // 2. find backwall
    uint16_t margin = gateA_b + (pulseWidth*2/1000 / velocity * fs); // margin of pulse, i.e. minimum resolution of scan
 
    uint16_t gateB_f = argFirstLarger(arr, margin, margin + targetThickDataPoint, threshold);
    if(gateB_f == 0)
        return -3;

    uint16_t gateB_b = argFirstSmaller(arr,gateB_f, margin + targetThickDataPoint, threshold);
    if(gateB_b == 0)
        return -4;

    uint16_t gateB = (gateB_f + gateB_b)/2;
    
    // 3. return calculated thickness
    std::cout << "GateA_f: " << gateA_f << " GateA_b: " << gateA_b << " GateB_f: " << gateB_f << " GateB_b: " << gateB_b << std::endl;
    return (gateB - gateA) / 2.0 / fs * velocity *1000;
}

typedef struct AscanFeatures
{
    float max;
    float min;
    float mean;
    float std;
    float rms;
};

//@brief C function to output parameters of input Asan
//@param arr Ptr to the float array containing ABSOLUTE Ascan data; features AscanFeatures struct containing calculated Ascan features
// 
void getAscanFeatures(const float* arr, AscanFeatures &features, float fs, float id)
{   

    uint16_t waterGap = id/1000 / vel_water * fs;

    float _tempMax = 0;
    float _tempMin = 0;
    float _tempSum = 0;
    float _tempRMS = 0;
    float _tempStd = 0;

    for(size_t i = waterGap; i< dataLength; i++)
    {
        if(_tempMax <= arr[i])
            _tempMax = arr[i];
        if(_tempMin >= arr[i])
            _tempMin = arr[i];
        
        _tempRMS+=arr[i]*arr[i];
        _tempSum+=arr[i];
    }

    features.max = _tempMax;
    features.min = _tempMin;
    features.rms = sqrtf(_tempRMS / dataLength);
    features.mean = _tempSum / dataLength;

    for(size_t i=0; i< dataLength; i++)
    {
         _tempStd += powf(arr[i] - features.mean, 2); 
    }

    features.std = sqrtf(_tempStd / (dataLength - 1));
 
}

 
int main(int args, char** argc)
{

    float vel = 2293.0;
    float fs = 125e6;
    float id = 150;
    float targetThickness = 5;
    float _thres = 0.1;

    AscanFeatures features;

    if(args > 1)
    {
        std::string path = argc[1];
        if(args == 7)
        {
            vel = atof(argc[2]);
            fs = atof(argc[3]);
            id = atof(argc[4]);
            targetThickness = atof(argc[5]);
            _thres = atof(argc[6]);
        }
        
        float data[dataLength]{0};

        std::ifstream file(path, std::ios::binary);  
        file.read(reinterpret_cast<char*>(data), dataLength*sizeof(float));

        for(auto&i : data)
            i = fabs(i);
        
        file.close();
        
        getAscanFeatures(data, features,fs,id);

        std::cout << thickCal(data, vel, fs, id, targetThickness, _thres) << std::endl;
        std::cout << "Ascan Features: \n"
                  << "Max: " << features.max << '\n'
                  << "Min: " << features.min << '\n'
                  << "Mean: " << features.mean << '\n'
                  << "Std: " << features.std << '\n'
                  << "RMS: " << features.rms << '\n';
    }

    return 0;
}

