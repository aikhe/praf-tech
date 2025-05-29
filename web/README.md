
# PRAF Technology - Flood Prevention System

A web application for PRAF Technology's flood prevention system that allows users to register their Philippine phone numbers (+63) to receive flood alerts.

## Setup Instructions

### Prerequisites
- Node.js
- npm or yarn

### Environment Variables
This project requires the following environment variables:

```
VITE_SUPABASE_URL=your_supabase_url
VITE_SUPABASE_ANON_KEY=your_supabase_anon_key
```

### Database Setup
You'll need to create a 'phone_numbers' table in your Supabase database with the following structure:

```sql
CREATE TABLE phone_numbers (
  id SERIAL PRIMARY KEY,
  phone_number TEXT NOT NULL,
  name TEXT NOT NULL,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);
```

### Installation

1. Clone the repository
2. Install dependencies: `npm install`
3. Create a `.env` file with the required environment variables
4. Run the development server: `npm run dev`

### Deployment to Cloudflare Pages

1. Connect your GitHub repository to Cloudflare Pages
2. Set the build command to `npm run build`
3. Set the build output directory to `dist`
4. Add the environment variables to your Cloudflare Pages project
5. Deploy!

## Features
- User registration with Philippines phone number validation
- Supabase database integration
- Responsive design
- PRAF Technology branding
