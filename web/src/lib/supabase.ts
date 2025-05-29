import { createClient } from "@supabase/supabase-js";

// Get the environment variables for Supabase configuration
const supabaseUrl =
  import.meta.env.VITE_SUPABASE_URL ||
  "https://jursmglsfqaqrxvirtiw.supabase.co";
const supabaseAnonKey =
  import.meta.env.VITE_SUPABASE_ANON_KEY ||
  "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imp1cnNtZ2xzZnFhcXJ4dmlydGl3Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDQ3ODkxOTEsImV4cCI6MjA2MDM2NTE5MX0.ajGbf9fLrYAA0KXzYhGFCTju-d4h-iTYTeU5WfITj3k";

// Display warning in development about missing environment variables
if (
  !import.meta.env.VITE_SUPABASE_URL ||
  !import.meta.env.VITE_SUPABASE_ANON_KEY
) {
  console.warn(
    "Using hardcoded Supabase credentials from the PRAF Flood Alert Hub device. For production, please set VITE_SUPABASE_URL and VITE_SUPABASE_ANON_KEY in your .env file.",
  );
}

// Create Supabase client
export const supabase = createClient(supabaseUrl, supabaseAnonKey);

// Phone number validation for Philippines (+63)
export const validatePhoneNumber = (phoneNumber: string): boolean => {
  // Check if the phone number starts with +63 and has the correct length
  const regex = /^\+63[0-9]{10}$/;
  return regex.test(phoneNumber);
};

// Register a phone number in the database
export const registerPhoneNumber = async (
  phoneNumber: string,
  name: string,
) => {
  if (!validatePhoneNumber(phoneNumber)) {
    throw new Error(
      "Invalid phone number format. Must start with +63 followed by 10 digits.",
    );
  }

  try {
    // Check if the phone number already exists
    const { data: existingData } = await supabase
      .from("phone_numbers")
      .select("phone_number")
      .eq("phone_number", phoneNumber)
      .limit(1);

    if (existingData && existingData.length > 0) {
      // Just update the name without trying to update a timestamp
      const { data, error } = await supabase
        .from("phone_numbers")
        .update({ name })
        .eq("phone_number", phoneNumber);

      if (error) throw error;
      return { success: true, data, updated: true };
    }

    // Insert new record
    const { data, error } = await supabase
      .from("phone_numbers")
      .insert([
        {
          phone_number: phoneNumber,
          name,
          created_at: new Date().toISOString(),
        },
      ]);

    if (error) throw error;

    return { success: true, data, updated: false };
  } catch (error) {
    console.error("Error registering phone number:", error);
    throw error;
  }
};
