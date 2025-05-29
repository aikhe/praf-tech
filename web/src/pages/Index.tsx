import Header from "@/components/Header";
import PhoneRegistrationForm from "@/components/PhoneRegistrationForm";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import {
  DropletIcon,
  MapPin,
  CloudSun,
  AlertTriangle,
  Info,
  Volume2,
  Smartphone,
  WifiIcon,
} from "lucide-react";

const Index = () => {
  return (
    <div className="min-h-screen flex flex-col">
      <Header />

      {/* Hero Section */}
      <section className="pt-24 pb-12 md:py-32 flood-gradient text-white relative overflow-hidden">
        <div className="water-pattern absolute bottom-0 left-0 right-0 h-8 opacity-50"></div>
        <div className="container px-4 md:px-6 flex flex-col md:flex-row items-center">
          <div className="flex-1 space-y-4 text-center md:text-left">
            <h1 className="text-3xl md:text-5xl font-bold tracking-tighter">
              Real-time Flood Monitoring & Alert System
            </h1>
            <p className="text-lg md:text-xl text-white/90 max-w-[600px]">
              PRAF Technology's advanced flood monitoring system uses ultrasonic
              sensors and real-time weather data to keep you informed with SMS
              alerts and audio warnings.
            </p>
            <div className="flex flex-col sm:flex-row gap-3 justify-center md:justify-start">
              <Button
                variant="outline"
                className="text-sky-600 border-white hover:bg-white/20 hover:text-white"
                size="lg"
                onClick={() =>
                  document
                    .getElementById("about")
                    ?.scrollIntoView({ behavior: "smooth" })
                }
              >
                Learn More
              </Button>
              <Button
                variant="outline"
                className="text-sky-600 border-white hover:bg-white/20 hover:text-white"
                size="lg"
                onClick={() =>
                  document
                    .getElementById("how-our-system-works")
                    ?.scrollIntoView({ behavior: "smooth" })
                }
              >
                How It Works
              </Button>
            </div>
          </div>
          <div className="flex-1 mt-8 md:mt-0 md:ml-8">
            <div className="relative max-w-md mx-auto md:mr-0">
              <div className="absolute -inset-1 bg-white/20 rounded-lg blur-sm"></div>
              <div className="relative bg-white/10 backdrop-blur-sm rounded-lg p-1">
                <PhoneRegistrationForm />
              </div>
            </div>
          </div>
        </div>
      </section>

      {/* About Section */}
      <section id="about" className="py-16 bg-background">
        <div className="container px-4 md:px-6">
          <div className="text-center mb-12">
            <h2 className="text-3xl font-bold text-sky-600">
              About PRAF Tech
            </h2>
            <p className="mt-2 text-muted-foreground">
              (Priority Rescue Assistance For Flooding) <br/> An ESP32-based
              smart monitoring system for flood detection and emergency alerts
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
            <div className="space-y-4">
              <div className="bg-sky-100 p-3 inline-block rounded-full mb-2">
                <AlertTriangle className="h-6 w-6 text-sky-600" />
              </div>
              <h3 className="text-2xl font-bold">Our Mission</h3>
              <p className="text-muted-foreground">
                At PRAF Technology, we aim to protect communities from dangerous
                flood conditions by providing advanced, reliable early warning
                systems. Our device leverages ultrasonic sensor technology and
                cloud connectivity to keep people informed with real-time alerts
                and multi-level warnings.
              </p>
            </div>
            <div className="space-y-4">
              <div className="bg-sky-100 p-3 inline-block rounded-full mb-2">
                <Info className="h-6 w-6 text-sky-600" />
              </div>
              <h3 className="text-2xl font-bold">How We Help</h3>
              <p className="text-muted-foreground">
                Our system monitors water levels in real-time using ultrasonic
                sensors and sends personalized SMS alerts based on three
                distinct danger levels. The device also provides local audio
                warnings, visual LED indicators, and displays vital information
                on an LCD screen - all while incorporating AI-powered weather
                forecasts.
              </p>
            </div>
          </div>
        </div>
      </section>

      {/* Features Section */}
      <section id="how-our-system-works" className="py-16 bg-sky-50">
        <div className="container px-4 md:px-6">
          <div className="text-center mb-12">
            <h2 className="text-3xl font-bold text-sky-600">
              Advanced Technology Features
            </h2>
            <p className="mt-2 text-muted-foreground">
              Multi-layered flood detection and alert system for community
              safety
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
            <Card className="border-sky-200/50 shadow-md bg-gradient-to-br from-white to-sky-50/50">
              <CardContent className="pt-6 flex flex-col items-center text-center">
                <div className="bg-sky-100 p-3 rounded-full mb-4">
                  <DropletIcon className="h-8 w-8 text-sky-600" />
                </div>
                <h3 className="text-xl font-bold mb-2">
                  Precision Water Sensing
                </h3>
                <p className="text-muted-foreground">
                  Our HC-SR04 ultrasonic sensor accurately detects rising water
                  levels and triggers appropriate alerts based on three distinct
                  danger thresholds.
                </p>
              </CardContent>
            </Card>

            <Card className="border-sky-200/50 shadow-md bg-gradient-to-br from-white to-sky-50/50">
              <CardContent className="pt-6 flex flex-col items-center text-center">
                <div className="bg-sky-100 p-3 rounded-full mb-4">
                  <Volume2 className="h-8 w-8 text-sky-600" />
                </div>
                <h3 className="text-xl font-bold mb-2">Multi-Modal Alerts</h3>
                <p className="text-muted-foreground">
                  Receive alerts through multiple channels: SMS messages, audio
                  warnings via built-in speakers, visual LED indicators, and LCD
                  screen updates.
                </p>
              </CardContent>
            </Card>

            <Card className="border-sky-200/50 shadow-md bg-gradient-to-br from-white to-sky-50/50">
              <CardContent className="pt-6 flex flex-col items-center text-center">
                <div className="bg-sky-100 p-3 rounded-full mb-4">
                  <CloudSun className="h-8 w-8 text-sky-600" />
                </div>
                <h3 className="text-xl font-bold mb-2">
                  AI Weather Integration
                </h3>
                <p className="text-muted-foreground">
                  Real-time weather data combined with Google's Gemini AI for
                  personalized, location-specific forecasts and safety
                  recommendations in Tagalog.
                </p>
              </CardContent>
            </Card>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-6 mt-6">
            <Card className="border-sky-200/50 shadow-md bg-gradient-to-br from-white to-sky-50/50">
              <CardContent className="pt-6 flex flex-col items-center text-center">
                <div className="bg-sky-100 p-3 rounded-full mb-4">
                  <Smartphone className="h-8 w-8 text-sky-600" />
                </div>
                <h3 className="text-xl font-bold mb-2">Targeted SMS Alerts</h3>
                <p className="text-muted-foreground">
                  The system sends detailed SMS alerts to registered phone
                  numbers with current flood levels, weather information, and
                  safety instructions.
                </p>
              </CardContent>
            </Card>

            <Card className="border-sky-200/50 shadow-md bg-gradient-to-br from-white to-sky-50/50">
              <CardContent className="pt-6 flex flex-col items-center text-center">
                <div className="bg-sky-100 p-3 rounded-full mb-4">
                  <MapPin className="h-8 w-8 text-sky-600" />
                </div>
                <h3 className="text-xl font-bold mb-2">Location Awareness</h3>
                <p className="text-muted-foreground">
                  Automatically detects your location to provide geographically
                  relevant weather forecasts and flood risk assessments.
                </p>
              </CardContent>
            </Card>

            <Card className="border-sky-200/50 shadow-md bg-gradient-to-br from-white to-sky-50/50">
              <CardContent className="pt-6 flex flex-col items-center text-center">
                <div className="bg-sky-100 p-3 rounded-full mb-4">
                  <WifiIcon className="h-8 w-8 text-sky-600" />
                </div>
                <h3 className="text-xl font-bold mb-2">Cloud Database</h3>
                <p className="text-muted-foreground">
                  Secure Supabase cloud integration for storing registered phone
                  numbers and ensuring reliable message delivery during
                  emergencies.
                </p>
              </CardContent>
            </Card>
          </div>
        </div>
      </section>

      {/* Alert Levels Section */}
      <section className="py-16 bg-white">
        <div className="container px-4 md:px-6">
          <div className="text-center mb-12">
            <h2 className="text-3xl font-bold text-sky-600">
              Three-Tier Alert System
            </h2>
            <p className="mt-2 text-muted-foreground">
              Our system provides graduated warnings based on precise water
              level measurements
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-8">
            <div className="border border-amber-200 bg-amber-50 rounded-lg p-6 relative overflow-hidden">
              <div className="absolute top-0 right-0 w-20 h-20 bg-amber-200/50 rounded-bl-full"></div>
              <h3 className="text-xl font-bold text-amber-600 mb-3">
                Level 1: Alert
              </h3>
              <p className="text-sm mb-3">Water level: 20-40cm</p>
              <ul className="space-y-2 text-sm">
                <li className="flex items-start gap-2">
                  <span className="bg-amber-200 p-1 rounded-full mt-0.5">
                    <AlertTriangle className="h-3 w-3 text-amber-700" />
                  </span>
                  <span>Initial flooding detected</span>
                </li>
                <li className="flex items-start gap-2">
                  <span className="bg-amber-200 p-1 rounded-full mt-0.5">
                    <AlertTriangle className="h-3 w-3 text-amber-700" />
                  </span>
                  <span>Yellow LED indicator</span>
                </li>
                <li className="flex items-start gap-2">
                  <span className="bg-amber-200 p-1 rounded-full mt-0.5">
                    <AlertTriangle className="h-3 w-3 text-amber-700" />
                  </span>
                  <span>Alert-level audio warning</span>
                </li>
              </ul>
            </div>

            <div className="border border-orange-200 bg-orange-50 rounded-lg p-6 relative overflow-hidden">
              <div className="absolute top-0 right-0 w-20 h-20 bg-orange-200/50 rounded-bl-full"></div>
              <h3 className="text-xl font-bold text-orange-600 mb-3">
                Level 2: Critical
              </h3>
              <p className="text-sm mb-3">Water level: 10-20cm</p>
              <ul className="space-y-2 text-sm">
                <li className="flex items-start gap-2">
                  <span className="bg-orange-200 p-1 rounded-full mt-0.5">
                    <AlertTriangle className="h-3 w-3 text-orange-700" />
                  </span>
                  <span>Significant flooding detected</span>
                </li>
                <li className="flex items-start gap-2">
                  <span className="bg-orange-200 p-1 rounded-full mt-0.5">
                    <AlertTriangle className="h-3 w-3 text-orange-700" />
                  </span>
                  <span>Orange LED indicator</span>
                </li>
                <li className="flex items-start gap-2">
                  <span className="bg-orange-200 p-1 rounded-full mt-0.5">
                    <AlertTriangle className="h-3 w-3 text-orange-700" />
                  </span>
                  <span>Critical-level audio warning</span>
                </li>
              </ul>
            </div>

            <div className="border border-red-200 bg-red-50 rounded-lg p-6 relative overflow-hidden">
              <div className="absolute top-0 right-0 w-20 h-20 bg-red-200/50 rounded-bl-full"></div>
              <h3 className="text-xl font-bold text-red-600 mb-3">
                Level 3: Warning
              </h3>
              <p className="text-sm mb-3">Water level: &lt;10cm</p>
              <ul className="space-y-2 text-sm">
                <li className="flex items-start gap-2">
                  <span className="bg-red-200 p-1 rounded-full mt-0.5">
                    <AlertTriangle className="h-3 w-3 text-red-700" />
                  </span>
                  <span>Severe flooding detected</span>
                </li>
                <li className="flex items-start gap-2">
                  <span className="bg-red-200 p-1 rounded-full mt-0.5">
                    <AlertTriangle className="h-3 w-3 text-red-700" />
                  </span>
                  <span>Red LED indicator</span>
                </li>
                <li className="flex items-start gap-2">
                  <span className="bg-red-200 p-1 rounded-full mt-0.5">
                    <AlertTriangle className="h-3 w-3 text-red-700" />
                  </span>
                  <span>Warning-level audio warning</span>
                </li>
              </ul>
            </div>
          </div>
        </div>
      </section>

      {/* CTA Section */}
      <section id="services" className="py-16 bg-sky-100">
        <div className="container px-4 md:px-6 text-center">
          <h2 className="text-3xl font-bold text-sky-700 mb-4">
            Be Proactive, Not Reactive
          </h2>
          <p className="mb-8 max-w-2xl mx-auto text-foreground/80">
            Don't wait until it's too late. Register your phone number now to
            receive timely flood alerts with real-time water levels, weather
            conditions, and AI-powered safety recommendations.
          </p>
          <Button
            className="flood-gradient hover:opacity-90 transition-opacity"
            size="lg"
            onClick={() => window.scrollTo({ top: 0, behavior: "smooth" })}
          >
            Register Your Number
          </Button>
        </div>
      </section>

      {/* Footer */}
      <footer id="contact" className="py-8 bg-sky-900 text-white">
        <div className="container px-4 md:px-6">
          <div className="grid grid-cols-1 md:grid-cols-3 gap-8">
            <div>
              <h3 className="text-lg font-semibold mb-4">PRAF Technology</h3>
              <p className="text-white/80">
                Advanced flood monitoring with ESP32, HC-SR04 sensors, and
                Supabase integration for a safer community.
              </p>
            </div>
            <div>
              <h3 className="text-lg font-semibold mb-4">Quick Links</h3>
              <ul className="space-y-2">
                <li>
                  <a href="#" className="text-white/80 hover:text-white">
                    Home
                  </a>
                </li>
                <li>
                  <a href="#about" className="text-white/80 hover:text-white">
                    About
                  </a>
                </li>
                <li>
                  <a
                    href="#how-our-system-works"
                    className="text-white/80 hover:text-white"
                  >
                    How It Works
                  </a>
                </li>
                <li>
                  <a href="#contact" className="text-white/80 hover:text-white">
                    Contact
                  </a>
                </li>
              </ul>
            </div>
            <div>
              <h3 className="text-lg font-semibold mb-4">Contact Us</h3>
              <address className="text-white/80 not-italic">
                <p>Caloocan, Metro Manila, Philippines</p>
                <p>Email: info@praftech.com</p>
                <p>Phone: +63 9649 687 066</p>
              </address>
            </div>
          </div>
          <div className="border-t border-white/20 mt-8 pt-6 text-center text-white/60 text-sm">
            <p>
              &copy; {new Date().getFullYear()} PRAF Technology. All rights
              reserved.
            </p>
          </div>
        </div>
      </footer>
    </div>
  );
};

export default Index;
