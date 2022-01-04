#include <iostream>
#include <cstdlib>
#include <cmath>
#include <vector>
#include "raylib.h"
#include "raymath.h"
#define RAYGUI_SUPPORT_ICONS
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

bool DrawEQ(Rectangle bounds, float* values, int borderWidth, int numBands, float bandsPadding, int numLevels, float levelsPadding, bool enabled, bool drawValues) {

    float bandsWidth = (bounds.width - 2*borderWidth - bandsPadding*(numBands+1))/numBands;
    float levelHeight = (bounds.height - 2*borderWidth - levelsPadding*(numLevels+1))/numLevels;
    Rectangle littleLED = (Rectangle) {0.0,0.0,bandsWidth,levelHeight};
    float levelStep = 1.0/(float) numLevels;
    Vector2 mousePosition = (Vector2) {0.0,0.0};
    char text[6];
    bool updated=false; //We return true if new values were input

    GuiDrawRectangle(bounds,5,LIGHTGRAY,LIGHTGRAY);
    for (int i=0; i<numBands; i++) {
        littleLED.x=bounds.x+bandsPadding+borderWidth+(bandsPadding+bandsWidth)*i;
        for (int j=0;j<numLevels;j++) {
            littleLED.y=bounds.y+bounds.height-borderWidth-(levelsPadding+levelHeight)*(j+1);
            DrawRectangleRec(littleLED,(values[i]>j*levelStep*1.01)?ORANGE:DARKGRAY); //levelStep*1.01 so float comparison works OK
            if ( IsMouseButtonDown(MOUSE_LEFT_BUTTON) && enabled ) {
                mousePosition = GetMousePosition();
                updated=true;
                if (CheckCollisionPointRec(mousePosition,littleLED)) {
                    values[i]=(j+1)*levelStep;
                }
                //Else if the mouse is below our lowest LED but still within the bounding box of the EQ control, set the value to zero
                else if ( (mousePosition.x>=littleLED.x) && (mousePosition.x<=littleLED.x+littleLED.width) && (mousePosition.y>(bounds.y+bounds.height-levelsPadding-borderWidth)) && (mousePosition.y<=bounds.y+bounds.height) ) {
                    values[i]=0.0;
                }
            }
            if (drawValues) {
                snprintf(text,6,"%1.6f",values[i]);
                DrawText(text,littleLED.x,bounds.y+bounds.height+10,10,RED);
            }
        }
    }

return updated;
}

float VertSliderControl(Rectangle bounds,float value,float minVal, float maxVal, int divisions, int handleSize, bool sticky, bool drawValues) {

    if (divisions<1) divisions=1;
    DrawRectangleRec(bounds,LIGHTGRAY);
    DrawRectangleLinesEx(bounds,1,DARKGRAY);
    int tickstart=(bounds.x+bounds.width/4);
    int tickfinish=(bounds.x+3*bounds.width/4);
    float tickSpacing = (bounds.height)/(float)divisions;
    float range=(maxVal-minVal);
    int displacement = (bounds.height-handleSize)*((value-minVal)/range);
    Rectangle handle = {bounds.x+3,bounds.y+bounds.height-handleSize-displacement,bounds.width-6,handleSize}; //Left and right of handle are 3 pixels in from bounds
    char text[6];

    //Draw the line down the middle
    DrawLine(bounds.x+bounds.width/2,bounds.y,bounds.x+bounds.width/2,bounds.y+bounds.height,BLACK);

    //Draw ticks along that line
    for (int i=1;i<divisions;i++) {
        DrawLine(tickstart,bounds.y+(i*tickSpacing),tickfinish,bounds.y+(i*tickSpacing),BLACK);
    }
    //Draw the handle
    DrawRectangleRec(handle,DARKGRAY);

    //Draw the value if enabled
    if (drawValues) {
        snprintf(text,6,"%f",value);
        DrawText(text,bounds.x,bounds.y+bounds.height+handleSize,10,RED);
    }

    Vector2 MousePosition = GetMousePosition();
    if (CheckCollisionPointRec(MousePosition,bounds)) {
        DrawRectangleLinesEx(bounds,1,SKYBLUE);
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            value=minVal+range*(bounds.y+bounds.height-MousePosition.y)/(float)bounds.height;
            if (sticky) {
                if (MousePosition.y<bounds.y+handleSize+1) {
                    value=maxVal;
                }
                else if (MousePosition.y>bounds.y+bounds.height-handleSize-1) {
                    value=minVal;
                }
                else {
                    //Lazy but it works
                    int tickcounter=0;
                    float tickvalue=range/divisions;
                    float whittlevalue=value;
                    while (whittlevalue>minVal) {
                        whittlevalue-=tickvalue;
                        tickcounter++;
                    }
                    value=minVal+(tickcounter-1)*tickvalue;
                }
            } //End quantization of values
        }
    }

return value;
}

int PhaseToggleControl(Rectangle bounds, int phase) {
    char text[4];

    DrawRectangleRec(bounds,LIGHTGRAY);
    snprintf(text,4,"%d",phase*45);
    DrawText(text,bounds.x+1,bounds.y+1,10,BLACK);

    Vector2 MousePosition=GetMousePosition();
    if (CheckCollisionPointRec(MousePosition,bounds)&&IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        phase++;
        if (phase==8) phase=0;
    }

return phase;
}

//Harmonic Synth parameters
struct HSParams {
    float multipleFreq;
    float ampCoeff;
    int phaseShift;
};

void UpdateWaveFunction(float* samples, unsigned int sampleCount, unsigned int sampleRate, float HZ, std::vector<HSParams>& HC1, std::vector<HSParams>& HC2, bool normalize, float* volumeEnv, float vibratoHZ, float vibratoAmp) {

    float timearg = 0.0;
    float maxval=0.0;
    float phaseshift=0.0;
    float lerpfactor=0.0;
    float lerpInc=1.0/(float)sampleCount;
    std::vector<HSParams> HCMixed(HC1.size());

    for (unsigned int i=0;i<sampleCount;i++) {
        timearg=(2*PI*(float)i)/(float)sampleRate;  //Scales timearg to 2PI / second
        samples[i]=0.0;
        for (unsigned int j=0;j<HCMixed.size();j++) {
            HCMixed[j].ampCoeff=(1.0-lerpfactor)*HC1[j].ampCoeff+lerpfactor*HC2[j].ampCoeff;
            HCMixed[j].phaseShift=(1.0-lerpfactor)*HC1[j].phaseShift+lerpfactor*HC2[j].phaseShift;
            HCMixed[j].multipleFreq=(1.0-lerpfactor)*HC1[j].multipleFreq+lerpfactor*HC2[j].multipleFreq;
            phaseshift = (float)HCMixed[j].phaseShift*DEG2RAD/HZ;
            samples[i]+=volumeEnv[i]*HCMixed[j].ampCoeff*sin((timearg+phaseshift*45)*(HZ+(vibratoAmp*sin(timearg*vibratoHZ)))*HCMixed[j].multipleFreq);   //Check phase shift wrt frequency scaling?
        }
        if (abs(samples[i])>maxval) maxval=abs(samples[i]);
    lerpfactor+=lerpInc;
    }

    //normalize
    if (normalize) {
            for (unsigned int i=0;i<sampleCount;i++) {
                samples[i]/=maxval;
            }
    }

return;
}

int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 1200;
    const int screenHeight = 800;
    const int frameRate = 60;

    InitWindow(screenWidth, screenHeight, "Harmonic Synthesizer");
    SetTargetFPS(frameRate);

    InitAudioDevice();

    float samples[44100];
    float HZ=440.0;
    float mainVolume = 1.0;
    size_t NumberOfComponents = 15;
    float timescale=0.0;
    float timeIncPerFrame = 1.0/(float)frameRate;
    float volumeEnv[44100];
    size_t numberOfVolumePoints = 20;
    std::vector<float> VolumePoints(numberOfVolumePoints,1.0);

    std::vector<HSParams> HarmonicComponents1(NumberOfComponents,{0.0,0.0,0});
    HarmonicComponents1[1].ampCoeff=1.0;
    HarmonicComponents1[1].multipleFreq=1.0;
    HarmonicComponents1[1].phaseShift=0;

    std::vector<HSParams> HarmonicComponents2(NumberOfComponents,{0.0,0.0,0});
    HarmonicComponents2[1].ampCoeff=1.0;
    HarmonicComponents2[1].multipleFreq=1.0;
    HarmonicComponents2[1].phaseShift=0;

    std::vector<HSParams> HCMixed(NumberOfComponents,{0.0,0.0,0});

    //To generate a whole number of waves: 1 wavelength takes 1/HZ seconds, which is 44100/HZ samples.
    Wave computedWave;
    computedWave.sampleCount=44100;
    computedWave.sampleRate=44100;
    computedWave.sampleSize=32;
    computedWave.channels=1;
    computedWave.data=samples;

    //Initialize volume envelope to be 1.0 throughout
    for (unsigned int i=0;i<44100;i++){
        volumeEnv[i]=1.0;
    }

    //Initialize the wave samples based
    UpdateWaveFunction(samples,computedWave.sampleCount,computedWave.sampleRate,HZ,HarmonicComponents2,HarmonicComponents1,true,volumeEnv,0.0,0.0);

    //Set up audio stream
    SetAudioStreamBufferSizeDefault(44100*1);
    AudioStream astream = InitAudioStream(44100,32,1);
    UpdateAudioStream(astream,samples,44100);
    SetAudioStreamVolume(astream,mainVolume);
    PlayAudioStream(astream);

    //GUI Locations and variables
    //Waveform drawing area is X=0-ScreenWidth, Y = O-220
    bool PauseToggle=true;
    Rectangle gcWaveScale = (Rectangle) {10,250,80,25};
    int waveScale=1;
    char gcText[50];
    Rectangle gcHZControl = (Rectangle) {100,250,590,25};
    Rectangle gcVolumeControl = (Rectangle) {700,250,190,25};
    int gcVolume = 100; //GUI Control volume is 0-100, scaled to float mainVol 0.0-1.0;
    Rectangle gcPlayToggle = (Rectangle) {900,250,90,25};
    unsigned int Sidx1,Sidx2,SPerPage; //Sample index for graphing
    Rectangle gcEQ = {760,340,400,180};
    bool EQUpdated=false;
    char gcSmallText[10];

    bool AddEcho=false;
    float echoDelay=0.0;
    float echoStrength=0.0;
    int reverb=0;
    float vibratoHZ=0.0;
    float vibratoAmp=0.0;

    bool gcTextBoxEditMode=false;
    char filename[20];

    // Main game loop
    while (!WindowShouldClose())        // Detect window close button or ESC key
    {
        // Key input
        if (IsKeyDown(KEY_P)) PauseToggle=!PauseToggle;

        if (IsKeyDown(KEY_DOWN)) {
            HZ=HZ-1;
        }
        if (IsKeyDown(KEY_UP)) {
            HZ=HZ+1;
        }
        if (IsKeyDown(KEY_LEFT)) {
            waveScale--;
        }
        if (IsKeyDown(KEY_RIGHT)) {
            waveScale++;
        }

        //Update wave samples
        UpdateWaveFunction(samples,computedWave.sampleCount,computedWave.sampleRate,HZ,HarmonicComponents2,HarmonicComponents1,true,volumeEnv,vibratoHZ,vibratoAmp);

        //Add echo afterwards, as a post-wave synth effect
        for (unsigned int r=0;r<reverb;r++) {
            if (AddEcho) {
                unsigned int StartSamplesIn=computedWave.sampleCount*echoDelay;
                for (unsigned int i=computedWave.sampleCount-1;i>StartSamplesIn;i--) {
                    samples[i]=(1.0-echoStrength)*samples[i]+echoStrength*samples[i-StartSamplesIn];
                }
            }
        }

        timescale+=timeIncPerFrame;
        if (timescale>1.0) timescale=0.0;

        // Draw
        //----------------------------------------------------------------------------------

        if (IsAudioStreamProcessed(astream)) {
            PauseToggle=true;
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);

            //Draw Wave
            for (unsigned int i=0;i<screenWidth-1;i++) {
                Sidx1=(i*waveScale+1200*timescale*frameRate);
                Sidx2=((i+1)*waveScale+1200*timescale*frameRate);
                Sidx1=Sidx1%44100;
                Sidx2=Sidx2%44100;
                DrawLine(i,120+100*samples[Sidx1],i+1,120+100*samples[Sidx2],RED);
            }

            //Draw Scale, Offset, HZ, VOL, and Play Toggle controls
            GuiSpinner(gcWaveScale,"",&waveScale,1,computedWave.sampleCount/screenWidth,false);
            DrawText("Wave Scale",gcWaveScale.x,gcWaveScale.y+gcWaveScale.height+5,10,BLACK);

            HZ=GuiScrollBar(gcHZControl,HZ,1,8000);
            snprintf(gcSmallText,10,"HZ: %d",(int)HZ);
            DrawText(gcSmallText,gcHZControl.x,gcHZControl.y+gcHZControl.height+5,10,BLACK);

            gcVolume = GuiScrollBar(gcVolumeControl,gcVolume,0,100);
            mainVolume = (float)gcVolume/100.0;
            SetAudioStreamVolume(astream,mainVolume);
            snprintf(gcText,20,"Main Volume: %1.3f",mainVolume);
            DrawText(gcText,gcVolumeControl.x,gcVolumeControl.y+gcVolumeControl.height+5,10,BLACK);

            PauseToggle=GuiToggle(gcPlayToggle,"Play/Pause",PauseToggle);

            if (!PauseToggle && IsAudioStreamProcessed(astream)) {
            UpdateAudioStream(astream,samples,44100);
            }

            //Draw Harmnonic Synth tools
            //Draw Primary Harmonic Components
            DrawText("Harmonic Components of Sustained Tone",10,300,10,BLACK);

            for (unsigned int i=0;i<NumberOfComponents;i++) {
                sprintf(gcSmallText,"%d",i);
                DrawText(gcSmallText,10+(40*i),320,10,BLACK);
                HarmonicComponents1[i].ampCoeff=VertSliderControl({10+(40*i),340,20,100},HarmonicComponents1[i].ampCoeff,0.0,1.0,10,6,false,true);
                HarmonicComponents1[i].multipleFreq=i;
                HarmonicComponents1[i].phaseShift=PhaseToggleControl({10+40*i,470,20,20},HarmonicComponents1[i].phaseShift);
            }
            //And their convenience buttons
            //Reset
            if (GuiButton({10+40*NumberOfComponents+20,340,40,30},"Sine") ) {
                    HarmonicComponents1[0].ampCoeff=0.0;
                    HarmonicComponents1[0].phaseShift=0;
                    HarmonicComponents1[1].ampCoeff=1.0;
                    HarmonicComponents1[1].phaseShift=0;
                for (unsigned int i=2;i<NumberOfComponents;i++) {
                    HarmonicComponents1[i].ampCoeff=0.0;
                    HarmonicComponents1[i].phaseShift=0;
                }
            }
            if (GuiButton({10+40*NumberOfComponents+70,340,40,30},"Square") ) {
                    HarmonicComponents1[0].ampCoeff=0.0;
                    HarmonicComponents1[0].phaseShift=0;
                    HarmonicComponents1[1].ampCoeff=1.0;
                    HarmonicComponents1[1].phaseShift=0;
                for (unsigned int i=2;i<NumberOfComponents;i++) {
                    HarmonicComponents1[i].ampCoeff=(i%2)?(1.0/i):0;
                    HarmonicComponents1[i].phaseShift=0;
                }
            }
            if (GuiButton({10+40*NumberOfComponents+20,380,40,30},"Tri") ) {
                    HarmonicComponents1[0].ampCoeff=0.0;
                    HarmonicComponents1[0].phaseShift=0;
                    HarmonicComponents1[1].phaseShift=0;
                for (unsigned int i=2;i<NumberOfComponents;i++) {
                    HarmonicComponents1[i].ampCoeff=1.0/i;
                    HarmonicComponents1[i].phaseShift=0;
                }
            }
            if (GuiButton({10+40*NumberOfComponents+70,380,40,30},"Copy B") ) {
                for (unsigned int i=0;i<NumberOfComponents;i++) {
                    HarmonicComponents1[i].ampCoeff=HarmonicComponents2[i].ampCoeff;
                    HarmonicComponents1[i].phaseShift=HarmonicComponents2[i].phaseShift;
                }
            }

            //Draw Second Set of Harmonic Components
            DrawText("Harmonic Components of Attack Tone",10,520,10,BLACK);

            for (unsigned int i=0;i<NumberOfComponents;i++) {
                sprintf(gcSmallText,"%d",i);
                DrawText(gcSmallText,10+(40*i),540,10,BLACK);
                HarmonicComponents2[i].ampCoeff=VertSliderControl({10+(40*i),560,20,100},HarmonicComponents2[i].ampCoeff,0.0,1.0,10,6,false,true);
                HarmonicComponents2[i].multipleFreq=i;
                HarmonicComponents2[i].phaseShift=PhaseToggleControl({10+40*i,690,20,20},HarmonicComponents2[i].phaseShift);
            }
            //And their convenience buttons
            //Reset
            if (GuiButton({10+40*NumberOfComponents+20,560,40,30},"Sine") ) {
                    HarmonicComponents2[0].ampCoeff=0.0;
                    HarmonicComponents2[0].phaseShift=0;
                    HarmonicComponents2[1].ampCoeff=1.0;
                    HarmonicComponents2[1].phaseShift=0;
                for (unsigned int i=2;i<NumberOfComponents;i++) {
                    HarmonicComponents2[i].ampCoeff=0.0;
                    HarmonicComponents2[i].phaseShift=0;
                }
            }
            if (GuiButton({10+40*NumberOfComponents+70,560,40,30},"Square") ) {
                    HarmonicComponents2[0].ampCoeff=0.0;
                    HarmonicComponents2[0].phaseShift=0;
                    HarmonicComponents2[1].ampCoeff=1.0;
                    HarmonicComponents2[1].phaseShift=0;
                for (unsigned int i=2;i<NumberOfComponents;i++) {
                    HarmonicComponents2[i].ampCoeff=(i%2)?(1.0/i):0;
                    HarmonicComponents2[i].phaseShift=0;
                }
            }
            if (GuiButton({10+40*NumberOfComponents+20,600,40,30},"Tri") ) {
                    HarmonicComponents2[0].ampCoeff=0.0;
                    HarmonicComponents2[0].phaseShift=0;
                    HarmonicComponents2[1].phaseShift=0;
                for (unsigned int i=2;i<NumberOfComponents;i++) {
                    HarmonicComponents2[i].ampCoeff=1.0/i;
                    HarmonicComponents2[i].phaseShift=0;
                }
            }
            if (GuiButton({10+40*NumberOfComponents+70,600,40,30},"Copy A") ) {
                for (unsigned int i=0;i<NumberOfComponents;i++) {
                    HarmonicComponents2[i].ampCoeff=HarmonicComponents1[i].ampCoeff;
                    HarmonicComponents2[i].phaseShift=HarmonicComponents1[i].phaseShift;
                }
            }

            //Draw Volume Envelope EQ tool
            DrawText("Volume Envelope:",gcEQ.x,gcEQ.y-20,10,BLACK);
            EQUpdated = DrawEQ(gcEQ,&VolumePoints[0],5,numberOfVolumePoints,4,20,2,true,false);

            //Update volume envelope if control was used
            // SampleCount/(numberOfVolumePoints-1) = SamplesPerInterval
            if (EQUpdated) {
                unsigned int SamplesPerInterval = 44100 / (numberOfVolumePoints-1);
                float lerpfactor = 0.0;
                for (unsigned int i=0;i<numberOfVolumePoints-1;i++) {
                    for (unsigned int j=0;j<SamplesPerInterval;j++) {
                            lerpfactor = (float)j/SamplesPerInterval;
                            volumeEnv[i*SamplesPerInterval+j] = (1.0-lerpfactor)*VolumePoints[i]+lerpfactor*VolumePoints[i+1];
                    }
                }
            }

            //Draw Post Effects
            //Echo controls
            DrawText("Echo Effects:",760,540,10,BLACK);
            AddEcho=GuiToggle({760,560,40,30},"Echo",AddEcho);
            echoDelay=VertSliderControl({760,600,40,80},echoDelay,0.0,0.5,10,3,false,true);
            echoStrength=VertSliderControl({820,600,40,80},echoStrength,0.0,0.5,10,3,false,true);
            DrawText("Echo Length and Strength",760,700,10,BLACK);
            GuiSpinner({760,720,60,30},"",&reverb,1,8,false);
            DrawText("Reverb",760,760,10,BLACK);

            //Vibrato
            DrawText("Vibrato:",920,540,10,BLACK);
            vibratoHZ=VertSliderControl({920,560,40,120},vibratoHZ,0.0,5.0,10,3,false,true);
            vibratoAmp=VertSliderControl({980,560,40,120},vibratoAmp,0,2,10,3,false,true);
            DrawText("Vib. Rate and Range",920,700,10,BLACK);

            //Draw Filename input and save/load buttons
            gcTextBoxEditMode=!GuiTextBox({150,730,125,30},filename,16,gcTextBoxEditMode);
            DrawText("Enter filename with extension then press save",10,770,10,BLACK);
            if (GuiButton((Rectangle){ 10, 730, 125, 30 }, GuiIconText(RICON_FILE_SAVE, "Save File"))) {
                if (filename!=NULL) {
                    ExportWave(computedWave,filename);
                }
            }


        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseAudioStream(astream);
    CloseAudioDevice();
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
