#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>

int main() {
    // Define the target screen coordinates (X, Y) from the top-left corner
    CGPoint targetPosition1 = CGPointMake(500.0, 400.0);
    CGPoint targetPosition2 = CGPointMake(550.0, 450.0);
    CGPoint targetPosition3 = CGPointMake(4500.0, 350.0);
    CGPoint points[3] = {targetPosition1, targetPosition2, targetPosition3};
    // Move the mouse cursor instantly
    while (1){
        for (int i=0; i<2; i++){
            CGWarpMouseCursorPosition(points[i]);
        }
    }
   
    
    return 0;
}

