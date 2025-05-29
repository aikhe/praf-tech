import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Droplet } from "lucide-react";

export default function Header() {
  const [isMenuOpen, setIsMenuOpen] = useState(false);
  
  // Function to scroll to the top of the page (where registration form is)
  const scrollToRegistrationForm = () => {
    window.scrollTo({ top: 0, behavior: 'smooth' });
    // Close mobile menu if open
    if (isMenuOpen) setIsMenuOpen(false);
  };

  return (
    <header className="w-full border-b border-border bg-background/80 backdrop-blur-sm fixed top-0 z-50">
      <div className="container flex h-16 items-center justify-between">
        <div className="flex items-center gap-1">
          <div className="h-10 w-10 overflow-hidden flex items-center justify-center relative">
            <Droplet className="h-8 w-8 text-sky-500" />
          </div>
          <span className="font-bold text-xl">
            PRAF <span className="text-sky-500">Technology</span>
          </span>
        </div>

        {/* Mobile menu button */}
        <Button
          variant="ghost"
          className="md:hidden"
          onClick={() => setIsMenuOpen(!isMenuOpen)}
        >
          <svg
            xmlns="http://www.w3.org/2000/svg"
            width="24"
            height="24"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2"
            strokeLinecap="round"
            strokeLinejoin="round"
          >
            {isMenuOpen ? (
              <>
                <line x1="18" y1="6" x2="6" y2="18"></line>
                <line x1="6" y1="6" x2="18" y2="18"></line>
              </>
            ) : (
              <>
                <line x1="3" y1="12" x2="21" y2="12"></line>
                <line x1="3" y1="6" x2="21" y2="6"></line>
                <line x1="3" y1="18" x2="21" y2="18"></line>
              </>
            )}
          </svg>
        </Button>

        {/* Desktop navigation */}
        <nav className="hidden md:flex items-center gap-6">
          <a
            href="#"
            className="text-foreground/80 hover:text-foreground transition-colors"
          >
            Home
          </a>
          <a
            href="#about"
            className="text-foreground/80 hover:text-foreground transition-colors"
          >
            About
          </a>
          <a
            href="#how-our-system-works"
            className="text-foreground/80 hover:text-foreground transition-colors"
          >
            Features
          </a>
          <a
            href="#contact"
            className="text-foreground/80 hover:text-foreground transition-colors"
          >
            Contact
          </a>
          <Button 
            className="flood-gradient hover:opacity-90 transition-opacity"
            onClick={scrollToRegistrationForm}
          >
            Register Now
          </Button>
        </nav>

        {/* Mobile navigation */}
        {isMenuOpen && (
          <div className="absolute top-16 left-0 right-0 bg-background border-b border-border p-4 md:hidden">
            <nav className="flex flex-col space-y-4">
              <a
                href="#"
                className="text-foreground/80 hover:text-foreground transition-colors"
              >
                Home
              </a>
              <a
                href="#about"
                className="text-foreground/80 hover:text-foreground transition-colors"
              >
                About
              </a>
              <a
                href="#how-our-system-works"
                className="text-foreground/80 hover:text-foreground transition-colors"
              >
                Features
              </a>
              <a
                href="#contact"
                className="text-foreground/80 hover:text-foreground transition-colors"
              >
                Contact
              </a>
              <Button 
                className="flood-gradient hover:opacity-90 transition-opacity w-full"
                onClick={scrollToRegistrationForm}
              >
                Register Now
              </Button>
            </nav>
          </div>
        )}
      </div>
    </header>
  );
}
