import { useState, useRef, useEffect } from "react";
import { Button } from "@/components/ui/button";
import {
  Card,
  CardContent,
  CardDescription,
  CardFooter,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { useToast } from "@/components/ui/use-toast";
import { validatePhoneNumber, registerPhoneNumber } from "@/lib/supabase";
import { AlertTriangle, Smartphone, MapPin } from "lucide-react";
import ReactConfetti from "react-confetti";
import { createPortal } from "react-dom";

export default function PhoneRegistrationForm() {
  const [phoneNumber, setPhoneNumber] = useState("");
  const [name, setName] = useState("");
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [showConfetti, setShowConfetti] = useState(false);
  const [windowDimensions, setWindowDimensions] = useState({
    width: typeof window !== "undefined" ? window.innerWidth : 1200,
    height: typeof window !== "undefined" ? window.innerHeight : 800,
  });
  const { toast } = useToast();

  useEffect(() => {
    const handleResize = () => {
      setWindowDimensions({
        width: window.innerWidth,
        height: window.innerHeight,
      });
    };

    if (typeof window !== "undefined") {
      window.addEventListener("resize", handleResize);
      return () => window.removeEventListener("resize", handleResize);
    }
  }, []);

  const handlePhoneChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const input = e.target.value;

    // Remove any non-digit characters from the input
    const digitsOnly = input.replace(/\D/g, "");
    setPhoneNumber(digitsOnly);

    // Clear error message if valid
    const formattedNumber = "+63" + digitsOnly.replace(/^0/, ""); // Remove leading 0 if present
    if (validatePhoneNumber(formattedNumber)) {
      setError(null);
    }
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError(null);

    // Format phone number with +63 prefix for validation and storage
    // Remove leading 0 if present before adding +63 prefix
    const formattedNumber = "+63" + phoneNumber.replace(/^0/, "");
    console.log(name);
    console.log(formattedNumber);

    if (!validatePhoneNumber(formattedNumber)) {
      setError("Please enter a valid phone number (e.g., 09XXXXXXXXX)");
      return;
    }

    if (!name.trim()) {
      setError("Please enter your name");
      return;
    }

    setIsSubmitting(true);

    try {
      const result = await registerPhoneNumber(formattedNumber, name);

      // Check if this was an update or a new registration
      if (result.updated) {
        toast({
          title: "Information Updated!",
          description:
            "Your name and phone number have been updated in our system.",
          variant: "default",
        });
      } else {
        toast({
          title: "Registration Successful!",
          description:
            "Your phone number has been registered for PRAF flood alerts.",
          variant: "default",
        });
      }

      // Show confetti for both new registrations and updates
      setShowConfetti(true);

      // Hide confetti after 5 seconds
      setTimeout(() => {
        setShowConfetti(false);
      }, 5000);

      // Reset form
      setPhoneNumber("");
      setName("");
    } catch (error) {
      if (error instanceof Error) {
        setError(error.message);
      } else {
        setError("An unexpected error occurred. Please try again.");
      }
      toast({
        title: "Registration Failed",
        description:
          "There was a problem registering your number. Please try again.",
        variant: "destructive",
      });
    } finally {
      setIsSubmitting(false);
    }
  };

  return (
    <>
      {showConfetti &&
        typeof document !== "undefined" &&
        typeof window !== "undefined" &&
        createPortal(
          <div
            style={{
              position: "fixed",
              top: 0,
              left: 0,
              width: "100%",
              height: "100%",
              zIndex: 9999,
              pointerEvents: "none",
            }}
          >
            <ReactConfetti
              width={windowDimensions.width}
              height={windowDimensions.height}
              recycle={false}
              numberOfPieces={1000}
              gravity={0.2}
              initialVelocityY={20}
              tweenDuration={100}
            />
          </div>,
          document.body,
        )}
      <Card className="w-full max-w-md mx-auto shadow-lg border-brand-purple/20">
        <CardHeader className="space-y-1">
          <CardTitle className="text-2xl font-bold">
            Register for Flood Alerts
          </CardTitle>
          <CardDescription>
            Receive SMS warnings about flood conditions with real-time water
            levels and AI safety tips
          </CardDescription>
        </CardHeader>
        <CardContent>
          <form onSubmit={handleSubmit} className="space-y-4">
            <div className="space-y-2">
              <Label htmlFor="name" className="flex items-center gap-2">
                <span>Full Name</span>
              </Label>
              <Input
                id="name"
                placeholder="Enter your full name"
                value={name}
                onChange={(e) => setName(e.target.value)}
                required
                className="border-brand-purple/30 focus-visible:ring-brand-purple"
              />
            </div>

            <div className="space-y-2">
              <Label htmlFor="phone" className="flex items-center gap-2">
                <span>Mobile Number</span>
                <Smartphone className="h-4 w-4 text-sky-500" />
              </Label>
              <Input
                id="phone"
                type="tel"
                placeholder="09XXXXXXXXX"
                value={phoneNumber}
                onChange={handlePhoneChange}
                required
                className="border-brand-purple/30 focus-visible:ring-brand-purple"
              />
              <p className="text-xs text-muted-foreground flex items-center gap-1">
                <MapPin className="h-3 w-3" />
                <span>Mobile number format (e.g., 09XXXXXXXXX)</span>
              </p>
            </div>

            {error && (
              <div className="bg-destructive/10 p-3 rounded-md flex items-start gap-2 text-sm">
                <AlertTriangle className="h-5 w-5 text-destructive flex-shrink-0" />
                <p className="text-destructive">{error}</p>
              </div>
            )}

            <Button
              type="submit"
              className="w-full flood-gradient hover:opacity-90 transition-opacity"
              disabled={isSubmitting}
            >
              {isSubmitting ? "Registering..." : "Register"}
            </Button>

            <div className="text-xs text-center text-muted-foreground">
              Your number will only be used for sending emergency flood alerts,
              weather updates, and safety information.
            </div>
          </form>
        </CardContent>
      </Card>
    </>
  );
}
